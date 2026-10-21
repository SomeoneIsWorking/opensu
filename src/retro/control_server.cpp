#include "control_server.hpp"

#include <cmath>
#include <optional>
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
    case 400:
        return "Bad Request";
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

/// A stick posted as `[x, y]`, each from -1 to 1, on the pad's scale; false when malformed.
bool readStick(const nlohmann::json& posted, std::int32_t& x, std::int32_t& y) {
    if (!posted.is_array() || posted.size() != 2 || !posted[0].is_number() ||
        !posted[1].is_number()) {
        return false;
    }
    const double px = posted[0].get<double>();
    const double py = posted[1].get<double>();
    if (px < -1.0 || px > 1.0 || py < -1.0 || py > 1.0) {
        return false;
    }
    x = static_cast<std::int32_t>(std::lround(px * gamepad::virtualStick.max));
    y = static_cast<std::int32_t>(std::lround(py * gamepad::virtualStick.max));
    return true;
}

/// The body of `POST /input`: `{"buttons": ["r2"], "left": [x, y], "right": [x, y]}`, each
/// part optional; nothing when malformed or naming Guide, which belongs to the shell.
std::optional<ControlInput> readInput(const std::string& body) {
    const nlohmann::json posted = nlohmann::json::parse(body, nullptr, false);
    if (!posted.is_object()) {
        return std::nullopt;
    }
    ControlInput input;
    if (const auto buttons = posted.find("buttons"); buttons != posted.end()) {
        if (!buttons->is_array()) {
            return std::nullopt;
        }
        for (const nlohmann::json& name : *buttons) {
            const gamepad::Button button = name.is_string()
                                               ? gamepad::buttonNamed(name.get<std::string>())
                                               : gamepad::Button::None;
            if (button == gamepad::Button::None || button == gamepad::Button::Guide) {
                return std::nullopt;
            }
            input.buttons.push_back(button);
        }
    }
    if (const auto left = posted.find("left");
        left != posted.end() && !readStick(*left, input.sticks.leftX, input.sticks.leftY)) {
        return std::nullopt;
    }
    if (const auto right = posted.find("right");
        right != posted.end() && !readStick(*right, input.sticks.rightX, input.sticks.rightY)) {
        return std::nullopt;
    }
    return input;
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

std::optional<ControlInput> ControlServer::takeInput() {
    const std::lock_guard lock{mutex_};
    return std::exchange(input_, std::nullopt);
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
    if (path == "/input") {
        if (request.method != "POST") {
            return text(405, "Method Not Allowed", "POST only");
        }
        std::optional<ControlInput> input = readInput(request.body);
        if (!input) {
            return text(400, "Bad Request",
                        R"(expected {"buttons": [names], "left": [x, y], "right": [x, y]})");
        }
        const std::lock_guard lock{mutex_};
        input_ = std::move(input);
        return text(200, "OK", "held");
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
