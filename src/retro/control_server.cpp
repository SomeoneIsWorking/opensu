#include "control_server.hpp"

#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>
#include <unistd.h>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::retro {
namespace {

lucent::http::Response text(int status, std::string reason, std::string body) {
    return lucent::http::Response::text(status, std::move(reason), std::move(body));
}

std::string reasonPhrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 503:
        return "Service Unavailable";
    default:
        return "Internal Server Error";
    }
}

} // namespace

ControlServer::ControlServer(std::filesystem::path file, std::string core, std::string rom)
    : file_{std::move(file)}, core_{std::move(core)}, rom_{std::move(rom)},
      server_{lucent::http::ServerOptions{.port = 0,
                                          .listen_scope = lucent::http::ListenScope::Loopback},
              [this](const lucent::http::Request& request) {
                  return handle(request);
              }} {
}

ControlServer::~ControlServer() {
    {
        const std::lock_guard lock{mutex_};
        closing_ = true;
    }
    changed_.notify_all();
    if (server_.running()) {
        server_.stop();
    }
    if (announced_) {
        std::error_code error;
        std::filesystem::remove(file_, error);
    }
}

bool ControlServer::start() {
    if (!server_.start()) {
        lucent::error("retro", "the control channel cannot listen");
        return false;
    }
    const nlohmann::json announce{{"port", server_.port()}, {"pid", getpid()}};
    std::string error;
    if (!fileio::writeWhole(file_, announce.dump(), error)) {
        lucent::error("retro", "cannot announce the control channel: {}", error);
        server_.stop();
        return false;
    }
    announced_ = true;
    lucent::info("retro", "control channel on http://127.0.0.1:{}/ announced in {}", server_.port(),
                 file_.string());
    return true;
}

void ControlServer::publish(std::uint64_t frame, bool hasState) {
    const std::lock_guard lock{mutex_};
    frame_ = frame;
    hasState_ = hasState;
}

void ControlServer::serve(const std::function<ControlAnswer(ControlCommand)>& perform) {
    std::deque<std::shared_ptr<Pending>> waiting;
    {
        const std::lock_guard lock{mutex_};
        waiting.swap(queue_);
        for (const std::shared_ptr<Pending>& pending : waiting) {
            pending->taken = true;
        }
    }
    for (const std::shared_ptr<Pending>& pending : waiting) {
        ControlAnswer answer = perform(pending->command);
        {
            const std::lock_guard lock{mutex_};
            hasState_ = answer.hasState;
            pending->answer = std::move(answer);
            pending->answered = true;
        }
        changed_.notify_all();
    }
}

void ControlServer::reportLoadFailure(const std::filesystem::path& file,
                                      const std::string& reason) {
    const nlohmann::json report{{"error", reason}};
    std::string error;
    if (!fileio::writeWhole(file, report.dump(), error)) {
        lucent::error("retro", "cannot report the load failure: {}", error);
    }
}

lucent::http::Response ControlServer::run(ControlCommand command) {
    const auto pending = std::make_shared<Pending>();
    pending->command = command;
    std::unique_lock lock{mutex_};
    if (closing_) {
        return text(503, "Service Unavailable", "the game is closing");
    }
    queue_.push_back(pending);
    const auto settled = [this, &pending] {
        return pending->answered || closing_;
    };
    changed_.wait_for(lock, loopWait, settled);
    if (!pending->answered && !closing_) {
        if (!pending->taken) {
            std::erase(queue_, pending);
            return text(503, "Service Unavailable", "the game is not running frames");
        }
        // Taken: it runs now, so its answer is the truth.
        changed_.wait(lock, settled);
    }
    if (!pending->answered) {
        return text(503, "Service Unavailable", "the game is closing");
    }
    const ControlAnswer& answer = pending->answer;
    return text(answer.status, reasonPhrase(answer.status),
                answer.status == 200 ? std::string{"done"} : answer.reason);
}

lucent::http::Response ControlServer::handle(const lucent::http::Request& request) {
    const std::string_view path = request.path();
    if (path == "/status") {
        if (request.method != "GET") {
            return text(405, "Method Not Allowed", "GET only");
        }
        const std::lock_guard lock{mutex_};
        const nlohmann::json status{
            {"core", core_}, {"rom", rom_}, {"frame", frame_}, {"hasState", hasState_}};
        return lucent::http::Response::json(200, "OK", status.dump());
    }
    ControlCommand command{};
    if (path == "/state/save") {
        command = ControlCommand::SaveState;
    } else if (path == "/state/load") {
        command = ControlCommand::LoadState;
    } else if (path == "/reset") {
        command = ControlCommand::Reset;
    } else {
        return text(404, "Not Found", "no such route");
    }
    if (request.method != "POST") {
        return text(405, "Method Not Allowed", "POST only");
    }
    return run(command);
}

} // namespace opensu::retro
