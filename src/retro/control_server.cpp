#include "control_server.hpp"

#include <cmath>
#include <optional>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "launch/process_id.hpp"
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

/// The body of `POST /touch`: `{"finger": 1, "phase": "down", "x": 120, "y": 640}`, in the target's
/// pixels; nothing when malformed.
std::optional<touch::Finger> readFinger(const std::string& body) {
    const nlohmann::json posted = nlohmann::json::parse(body, nullptr, false);
    if (!posted.is_object() || !posted.contains("finger") ||
        !posted["finger"].is_number_unsigned() || !posted.contains("phase") ||
        !posted["phase"].is_string() || !posted.contains("x") || !posted["x"].is_number() ||
        !posted.contains("y") || !posted["y"].is_number()) {
        return std::nullopt;
    }
    touch::Finger finger{.id = posted["finger"].get<std::uint64_t>(),
                         .x = posted["x"].get<float>(),
                         .y = posted["y"].get<float>()};
    const std::string phase = posted["phase"].get<std::string>();
    if (phase == "down") {
        finger.phase = touch::Finger::Phase::Down;
    } else if (phase == "move") {
        finger.phase = touch::Finger::Phase::Move;
    } else if (phase == "up") {
        finger.phase = touch::Finger::Phase::Up;
    } else {
        return std::nullopt;
    }
    return finger;
}

/// The body of `POST /host`: `{"covered": true}`, `{"pads": 1}`, `{"key": {"scancode": 4, "down":
/// true}}` or `{"screen": {"safeX": 0, "safeY": 0, "safeWidth": 1280, "safeHeight": 800, "dp":
/// 2}}`, any of them together; nothing when malformed.
std::optional<HostInput> readHost(const std::string& body) {
    const nlohmann::json posted = nlohmann::json::parse(body, nullptr, false);
    if (!posted.is_object()) {
        return std::nullopt;
    }
    HostInput host;
    if (const auto covered = posted.find("covered"); covered != posted.end()) {
        if (!covered->is_boolean()) {
            return std::nullopt;
        }
        host.covered = covered->get<bool>();
    }
    if (const auto pads = posted.find("pads"); pads != posted.end()) {
        if (!pads->is_number_unsigned()) {
            return std::nullopt;
        }
        host.pads = pads->get<unsigned>();
    }
    if (const auto key = posted.find("key"); key != posted.end()) {
        if (!key->is_object() || !key->contains("scancode") ||
            !(*key)["scancode"].is_number_integer() || !key->contains("down") ||
            !(*key)["down"].is_boolean()) {
            return std::nullopt;
        }
        host.key = HostInput::Key{(*key)["scancode"].get<int>(), (*key)["down"].get<bool>()};
    }
    if (const auto screen = posted.find("screen"); screen != posted.end()) {
        touch::WindowSurface placed;
        const std::pair<const char*, float*> parts[] = {{"safeX", &placed.safeX},
                                                        {"safeY", &placed.safeY},
                                                        {"safeWidth", &placed.safeWidth},
                                                        {"safeHeight", &placed.safeHeight},
                                                        {"dp", &placed.dp}};
        if (!screen->is_object()) {
            return std::nullopt;
        }
        for (const auto& [name, value] : parts) {
            if (!screen->contains(name) || !(*screen)[name].is_number()) {
                return std::nullopt;
            }
            *value = (*screen)[name].get<float>();
        }
        if (placed.dp <= 0.0F) {
            return std::nullopt;
        }
        host.screen = placed;
    }
    return host;
}

/// The body of `POST /input`: `{"pad": "two", "buttons": ["r2"], "left": [x, y], "right": [x, y]}`
/// or `{"pad": "two", "leave": true}`, each part optional; nothing when malformed or naming Guide,
/// which belongs to the shell.
std::optional<ControlInput> readInput(const std::string& body) {
    const nlohmann::json posted = nlohmann::json::parse(body, nullptr, false);
    if (!posted.is_object()) {
        return std::nullopt;
    }
    ControlInput input;
    if (const auto pad = posted.find("pad"); pad != posted.end()) {
        if (!pad->is_string() || pad->get<std::string>().empty()) {
            return std::nullopt;
        }
        input.pad = pad->get<std::string>();
    }
    if (const auto leave = posted.find("leave"); leave != posted.end()) {
        if (!leave->is_boolean()) {
            return std::nullopt;
        }
        input.leave = leave->get<bool>();
    }
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

bool readOption(const std::string& body, std::string& key, std::string& value) {
    const nlohmann::json posted = nlohmann::json::parse(body, nullptr, false);
    if (!posted.is_object() || !posted.contains("key") || !posted.contains("value") ||
        !posted["key"].is_string() || !posted["value"].is_string()) {
        return false;
    }
    key = posted["key"].get<std::string>();
    value = posted["value"].get<std::string>();
    return !key.empty();
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
    // A Rotate screen choice or a failure stays in the file for the shell, which removes it.
    if (announced_ && !turned_ && !failed_) {
        std::error_code error;
        std::filesystem::remove(file_, error);
    }
}

std::string ControlServer::announcement() const {
    nlohmann::json announce{{"port", server_.port()}, {"pid", launch::ownProcessId()}};
    if (netplayPort_) {
        announce["netplayPort"] = *netplayPort_;
    }
    if (turned_) {
        announce["turned"] = *turned_;
    }
    return announce.dump();
}

void ControlServer::noteTurned(bool turned) {
    turned_ = turned;
    std::string error;
    if (!fileio::writeWhole(file_, announcement(), error)) {
        lucent::error("retro", "cannot keep the screen's turn: {}", error);
    }
}

bool ControlServer::start() {
    if (!server_.start()) {
        lucent::error("retro", "the control channel cannot listen");
        return false;
    }
    std::string error;
    if (!fileio::writeWhole(file_, announcement(), error)) {
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

void ControlServer::reportPicture(const ShownPicture& picture) {
    const std::lock_guard lock{mutex_};
    picture_ = picture;
}

bool ControlServer::takeNative() {
    const std::lock_guard lock{mutex_};
    return std::exchange(nativeAsked_, false);
}

void ControlServer::reportPanel(std::string panel) {
    const std::lock_guard lock{mutex_};
    panel_ = std::move(panel);
}

void ControlServer::reportShown() {
    const std::lock_guard lock{mutex_};
    shown_ = true;
}

void ControlServer::reportResume(const ResumeReport& report) {
    const std::lock_guard lock{mutex_};
    resume_ = report;
}

void ControlServer::serve(const std::function<ControlAnswer(const ControlRequest&)>& perform) {
    std::deque<std::shared_ptr<Pending>> waiting;
    {
        const std::lock_guard lock{mutex_};
        waiting.swap(queue_);
        for (const std::shared_ptr<Pending>& pending : waiting) {
            pending->taken = true;
        }
    }
    for (const std::shared_ptr<Pending>& pending : waiting) {
        ControlAnswer answer = perform(pending->request);
        {
            const std::lock_guard lock{mutex_};
            hasState_ = answer.hasState;
            pending->answer = std::move(answer);
            pending->answered = true;
        }
        changed_.notify_all();
    }
}

std::vector<touch::Finger> ControlServer::takeFingers() {
    const std::lock_guard lock{mutex_};
    return std::exchange(fingers_, {});
}

std::vector<HostInput> ControlServer::takeHost() {
    const std::lock_guard lock{mutex_};
    return std::exchange(host_, {});
}

void ControlServer::publishTouch(std::string places) {
    const std::lock_guard lock{mutex_};
    touchPlaces_ = std::move(places);
}

std::vector<ControlInput> ControlServer::takeInput() {
    const std::lock_guard lock{mutex_};
    return std::exchange(input_, {});
}

void ControlServer::reportFailure(const std::filesystem::path& file, const std::string& reason) {
    const nlohmann::json report{{"error", reason}};
    std::string error;
    if (!fileio::writeWhole(file, report.dump(), error)) {
        lucent::error("retro", "cannot report the failure: {}", error);
    }
}

void ControlServer::reportFailure(const std::string& reason) {
    reportFailure(file_, reason);
    failed_ = true;
}

lucent::http::Response ControlServer::run(ControlRequest request) {
    const auto pending = std::make_shared<Pending>();
    pending->request = std::move(request);
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
    if (answer.status == 200 && !answer.png.empty()) {
        return lucent::http::Response::binary(200, "OK", "image/png", answer.png);
    }
    if (answer.status == 200 && !answer.json.empty()) {
        return lucent::http::Response::json(200, "OK", answer.json);
    }
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
        nlohmann::json status{{"core", core_},
                              {"rom", rom_},
                              {"frame", frame_},
                              {"shown", shown_},
                              {"panel", panel_},
                              {"pictureWidth", picture_.width},
                              {"pictureHeight", picture_.height},
                              {"pictureAspect", picture_.aspect},
                              {"pictureNearest", picture_.nearest},
                              {"hasState", hasState_}};
        if (resume_.from) {
            status["resumedFrom"] =
                std::chrono::duration_cast<std::chrono::seconds>(resume_.from->time_since_epoch())
                    .count();
        }
        if (!resume_.refused.empty()) {
            status["resumeRefused"] = resume_.refused;
        }
        return lucent::http::Response::json(200, "OK", status.dump());
    }
    if (path == "/native") {
        if (request.method != "POST") {
            return text(405, "Method Not Allowed", "POST only");
        }
        const std::lock_guard lock{mutex_};
        nativeAsked_ = true;
        return text(200, "OK", "queued");
    }
    if (path == "/input") {
        if (request.method != "POST") {
            return text(405, "Method Not Allowed", "POST only");
        }
        std::optional<ControlInput> input = readInput(request.body);
        if (!input) {
            return text(400, "Bad Request",
                        R"(expected {"pad": name, "buttons": [names], "left": [x, y], )"
                        R"("right": [x, y], "leave": true})");
        }
        const std::lock_guard lock{mutex_};
        input_.push_back(std::move(*input));
        return text(200, "OK", "held");
    }
    if (path == "/touch") {
        if (request.method == "GET") {
            const std::lock_guard lock{mutex_};
            return lucent::http::Response::json(200, "OK", touchPlaces_);
        }
        if (request.method != "POST") {
            return text(405, "Method Not Allowed", "GET or POST only");
        }
        const std::optional<touch::Finger> finger = readFinger(request.body);
        if (!finger) {
            return text(400, "Bad Request",
                        R"(expected {"finger": id, "phase": "down|move|up", "x": px, "y": px})");
        }
        const std::lock_guard lock{mutex_};
        fingers_.push_back(*finger);
        return text(200, "OK", "queued");
    }
    if (path == "/host") {
        if (request.method != "POST") {
            return text(405, "Method Not Allowed", "POST only");
        }
        std::optional<HostInput> host = readHost(request.body);
        if (!host) {
            return text(400, "Bad Request",
                        R"(expected {"covered": bool, "pads": n, "key": {...}, "screen": {...}})");
        }
        const std::lock_guard lock{mutex_};
        host_.push_back(*host);
        return text(200, "OK", "taken");
    }
    if (path == "/frame.png") {
        if (request.method != "GET") {
            return text(405, "Method Not Allowed", "GET only");
        }
        ControlRequest command;
        command.command = ControlCommand::Frame;
        return run(std::move(command));
    }
    if (path == "/options") {
        if (request.method != "GET") {
            return text(405, "Method Not Allowed", "GET only");
        }
        ControlRequest command;
        command.command = ControlCommand::Options;
        return run(std::move(command));
    }
    ControlRequest command;
    if (path == "/state/save") {
        command.command = ControlCommand::SaveState;
    } else if (path == "/state/load") {
        command.command = ControlCommand::LoadState;
    } else if (path == "/reset") {
        command.command = ControlCommand::Reset;
    } else if (path == "/video") {
        command.command = ControlCommand::Video;
    } else if (path == "/option") {
        command.command = ControlCommand::Option;
    } else if (path == "/suspend") {
        command.command = ControlCommand::Suspend;
    } else if (path == "/hold") {
        command.command = ControlCommand::Hold;
    } else if (path == "/release") {
        command.command = ControlCommand::Release;
    } else if (path == "/turn") {
        command.command = ControlCommand::Turn;
    } else if (path == "/quit") {
        command.command = ControlCommand::Quit;
    } else {
        return text(404, "Not Found", "no such route");
    }
    if (request.method != "POST") {
        return text(405, "Method Not Allowed", "POST only");
    }
    if (command.command == ControlCommand::Video) {
        std::string error;
        std::optional<settings::VideoSettings> video =
            settings::parseVideoJson(request.body, error);
        if (!video) {
            return text(400, "Bad Request", error);
        }
        command.video = std::move(*video);
    } else if (command.command == ControlCommand::Option &&
               !readOption(request.body, command.key, command.value)) {
        return text(400, "Bad Request", R"(expected {"key": "...", "value": "..."})");
    }
    return run(std::move(command));
}

} // namespace opensu::retro
