#include "embed_socket.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>
#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "launch/process_table.hpp"
#include "lucent/line_socket.h"
#include "lucent/log.h"

namespace opensu::app {
namespace {

using Json = nlohmann::json;

/// The file name of `pid`'s program, as the first word of its command line names it (a Wine
/// process's is its Windows path).
std::string programOf(int pid) {
    const std::string line = launch::table::commandLine(pid);
    const std::string first = line.substr(0, line.find('\0'));
    const std::size_t slash = first.find_last_of("/\\");
    return slash == std::string::npos ? first : first.substr(slash + 1);
}

bool isExplorer(std::string program) {
    std::ranges::transform(program, program.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return program == "explorer.exe";
}

double numberIn(const Json& message, const char* name) {
    const auto found = message.find(name);
    return found != message.end() && found->is_number() ? found->get<double>() : 0.0;
}

} // namespace

EmbedSocket::EmbedSocket(std::filesystem::path directory, std::function<std::string()> launchTree)
    : directory_{std::move(directory)}, launchTree_{std::move(launchTree)} {
}

EmbedSocket::~EmbedSocket() = default;

std::optional<std::string> EmbedSocket::open() {
    close();
    std::error_code made;
    std::filesystem::create_directories(directory_, made);
    const std::filesystem::path path =
        directory_ / ("embed-" + std::to_string(getpid()) + "-" + std::to_string(++launches_));
    std::string error;
    std::optional<lucent::net::LineSocketServer> server =
        lucent::net::LineSocketServer::listen(path, error);
    if (!server) {
        lucent::error("embed", "no socket for the game's windows at {}: {}", path.string(), error);
        return std::nullopt;
    }
    server_ = std::make_unique<lucent::net::LineSocketServer>(std::move(*server));
    return "OPENSU_EMBED_SOCKET=" + path.string();
}

void EmbedSocket::close() {
    server_.reset();
    windows_.clear();
    hellos_.clear();
    sawWindow_ = false;
    cursorHidden_ = false;
    focused_ = true;
}

bool EmbedSocket::gameSaidHello() const {
    return std::ranges::any_of(hellos_, [](const Hello& hello) {
        return !isExplorer(hello.program);
    });
}

bool EmbedSocket::poll() {
    if (!server_) {
        return false;
    }
    const std::vector<Window> before = windows_;
    for (const lucent::net::LineSocketServer::Event& event : server_->poll()) {
        using Kind = lucent::net::LineSocketServer::Event::Kind;
        if (event.kind == Kind::Line) {
            handle(event.peer, event.line);
        } else if (event.kind == Kind::Closed) {
            std::erase_if(windows_, [&](const Window& window) {
                return window.peer == event.peer;
            });
            std::erase_if(hellos_, [&](const Hello& hello) {
                return hello.peer == event.peer;
            });
        }
    }
    const auto same = [](const Window& a, const Window& b) {
        return a.id == b.id && a.context == b.context && a.x == b.x && a.y == b.y &&
               a.width == b.width && a.height == b.height && a.z == b.z;
    };
    return !std::ranges::equal(before, windows_, same);
}

void EmbedSocket::handle(std::uint32_t peer, std::string_view line) {
    const Json message = Json::parse(line, nullptr, false);
    if (!message.is_object()) {
        lucent::warn("embed", "a line that is not a JSON object: {}", std::string{line});
        return;
    }
    if (message.contains("hello")) {
        const int pid = static_cast<int>(numberIn(message, "pid"));
        Hello hello{.peer = peer, .pid = pid, .program = programOf(pid)};
        lucent::info("embed", "hello from {} ({})", hello.program, pid);
        hellos_.push_back(std::move(hello));
        sendTo(
            peer,
            Json{{"screen",
                  {{"width", screen_.width}, {"height", screen_.height}, {"scale", screen_.scale}}}}
                .dump());
        sendTo(peer, Json{{"focus", focused_}}.dump());
        return;
    }
    if (const auto cursor = message.find("cursor");
        cursor != message.end() && cursor->is_string()) {
        cursorHidden_ = cursor->get<std::string>() == "hidden";
        return;
    }
    const auto id = message.find("window");
    if (id == message.end() || !id->is_number_unsigned()) {
        return;
    }
    const std::uint64_t windowId = id->get<std::uint64_t>();
    std::erase_if(windows_, [&](const Window& window) {
        return window.id == windowId && window.peer == peer;
    });
    if (message.value("gone", false)) {
        return;
    }
    Window window{.id = windowId,
                  .peer = peer,
                  .context = static_cast<std::uint32_t>(numberIn(message, "context")),
                  .x = numberIn(message, "x"),
                  .y = numberIn(message, "y"),
                  .width = numberIn(message, "width"),
                  .height = numberIn(message, "height"),
                  .scale = message.contains("scale") ? numberIn(message, "scale") : 1.0,
                  .title = message.value("title", std::string{}),
                  .z = static_cast<int>(numberIn(message, "z")),
                  .kind = message.value("kind", std::string{"normal"})};
    if (!sawWindow_) {
        lucent::info("embed", "first window: \"{}\" {}x{}", window.title, window.width,
                     window.height);
    }
    sawWindow_ = true;
    windows_.push_back(std::move(window));
    std::ranges::stable_sort(windows_, {}, &Window::z);
}

void EmbedSocket::sendTo(std::uint32_t peer, const std::string& line) {
    if (server_) {
        server_->send(peer, line);
    }
}

std::optional<std::uint32_t> EmbedSocket::front() const {
    if (!windows_.empty()) {
        return windows_.back().peer;
    }
    for (const Hello& hello : hellos_) {
        if (!isExplorer(hello.program)) {
            return hello.peer;
        }
    }
    return std::nullopt;
}

void EmbedSocket::setScreen(Screen screen) {
    if (screen == screen_) {
        return;
    }
    screen_ = screen;
    if (server_) {
        server_->send_all(
            Json{{"screen",
                  {{"width", screen.width}, {"height", screen.height}, {"scale", screen.scale}}}}
                .dump());
    }
}

void EmbedSocket::setFocus(bool focused) {
    if (focused == focused_) {
        return;
    }
    focused_ = focused;
    if (server_) {
        server_->send_all(Json{{"focus", focused}}.dump());
    }
}

void EmbedSocket::key(const Key& key) {
    if (const std::optional<std::uint32_t> peer = front()) {
        sendTo(*peer, Json{{"key",
                            {{"down", key.down},
                             {"keycode", key.keycode},
                             {"chars", key.chars},
                             {"modifiers", key.modifiers},
                             {"repeat", key.repeat}}}}
                          .dump());
    }
}

void EmbedSocket::mouse(const Window& window, double x, double y, unsigned buttons) {
    sendTo(
        window.peer,
        Json{{"mouse", {{"window", window.id}, {"x", x}, {"y", y}, {"buttons", buttons}}}}.dump());
}

void EmbedSocket::scroll(const Window& window, double dx, double dy, bool precise) {
    sendTo(window.peer,
           Json{{"scroll", {{"window", window.id}, {"dx", dx}, {"dy", dy}, {"precise", precise}}}}
               .dump());
}

void EmbedSocket::relative(double dx, double dy) {
    if (const std::optional<std::uint32_t> peer = front()) {
        sendTo(*peer, Json{{"relative", {{"dx", dx}, {"dy", dy}}}}.dump());
    }
}

} // namespace opensu::app
