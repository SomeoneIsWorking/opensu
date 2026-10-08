#include "control_channel.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <string_view>

#include "lucent/log.h"

namespace iideck::app {
namespace {

/// Why a request was refused, so a caller is told the difference between "you
/// asked for something that does not exist" and "the shell cannot do that now".
lucent::http::Response refuse(std::string reason) {
    return lucent::http::Response::text(400, "Bad Request", std::move(reason));
}

lucent::http::Response notFound(std::string_view what) {
    return lucent::http::Response::text(404, "Not Found", std::string{what});
}

lucent::http::Response serverError(std::string reason) {
    return lucent::http::Response::text(500, "Internal Server Error", std::move(reason));
}

/// Quotes a JSON string, including the control characters a title may contain.
std::string jsonString(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (const char c : text) {
        switch (c) {
        case '"':
            out.append("\\\"");
            break;
        case '\\':
            out.append("\\\\");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                std::array<char, 8> escape{};
                std::snprintf(escape.data(), escape.size(), "\\u%04x",
                              static_cast<unsigned char>(c));
                out.append(escape.data());
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
    return out;
}

std::string jsonSnapshot(const ShellSnapshot& snapshot) {
    std::string out{"{"};
    const auto number = [&out](std::string_view key, std::size_t value, bool last = false) {
        out.append("\"").append(key).append("\":").append(std::to_string(value));
        out.append(last ? "}" : ",");
    };
    const auto text = [&out](std::string_view key, std::string_view value, bool last = false) {
        out.append("\"").append(key).append("\":").append(jsonString(value));
        out.append(last ? "}" : ",");
    };
    const auto flag = [&out](std::string_view key, bool value, bool last = false) {
        out.append("\"").append(key).append("\":").append(value ? "true" : "false");
        out.append(last ? "}" : ",");
    };

    number("games", snapshot.games);
    number("installed", snapshot.installed);
    number("focusIndex", snapshot.focusIndex);
    number("page", snapshot.page);
    number("pageCount", snapshot.pageCount);
    text("focusedId", snapshot.focusedId);
    text("focusedTitle", snapshot.focusedTitle);
    text("shelf", snapshot.shelf);
    text("status", snapshot.status);
    text("toast", snapshot.toast);
    flag("toastIsError", snapshot.toastIsError);
    flag("launching", snapshot.launching);
    flag("inGame", snapshot.inGame);
    flag("gameMenuOpen", snapshot.gameMenuOpen);
    text("steam", snapshot.steam);
    text("launchers", snapshot.launchers, true);
    return out;
}

} // namespace

bool parseButton(std::string_view name, gamepad::Button& out) {
    // The same spellings the shell's own hints use, so a caller driving the
    // shell by HTTP and a person reading a footer agree on what "a" means.
    static constexpr std::array<std::pair<std::string_view, gamepad::Button>, 13> kNames{{
        {"up", gamepad::Button::Up},
        {"down", gamepad::Button::Down},
        {"left", gamepad::Button::Left},
        {"right", gamepad::Button::Right},
        {"a", gamepad::Button::A},
        {"b", gamepad::Button::B},
        {"x", gamepad::Button::X},
        {"y", gamepad::Button::Y},
        {"l1", gamepad::Button::L1},
        {"r1", gamepad::Button::R1},
        {"select", gamepad::Button::Select},
        {"start", gamepad::Button::Start},
        {"guide", gamepad::Button::Guide},
    }};
    for (const auto& [spelling, button] : kNames) {
        if (spelling == name) {
            out = button;
            return true;
        }
    }
    return false;
}

ControlChannel::ControlChannel(ControlTarget& target, std::uint16_t port)
    : target_{target},
      server_{lucent::http::ServerOptions{.port = port,
                                          .listen_scope = lucent::http::ListenScope::Loopback},
              [this](const lucent::http::Request& request) {
                  return handle(request);
              }} {
}

ControlChannel::~ControlChannel() {
    stop();
}

bool ControlChannel::start() {
    if (!server_.start()) {
        // The shell is the product. A diagnostic channel that cannot bind is an
        // error worth reporting, not a reason to refuse to start.
        lucent::error("control", "could not bind the control channel on port {}", server_.port());
        return false;
    }
    lucent::info("control", "control channel on http://127.0.0.1:{}/ (loopback only)",
                 server_.port());
    return true;
}

void ControlChannel::stop() {
    if (server_.running()) {
        server_.stop();
    }
}

std::uint16_t ControlChannel::port() const noexcept {
    return server_.port();
}

lucent::http::Response ControlChannel::handle(const lucent::http::Request& request) {
    const std::string_view path = request.path();

    if (path == "/state") {
        if (request.method != "GET") {
            return refuse("GET only");
        }
        return lucent::http::Response::json(200, "OK", jsonSnapshot(target_.snapshot()));
    }

    if (path == "/input") {
        if (request.method != "POST") {
            return refuse("POST only");
        }
        // The body is the button name, so a caller can drive the shell with
        // `curl -d left`, with no JSON to parse on either side.
        gamepad::Button button{};
        std::string_view name{request.body};
        while (!name.empty() &&
               (name.back() == '\n' || name.back() == '\r' || name.back() == ' ')) {
            name.remove_suffix(1);
        }
        if (name.empty()) {
            return refuse(
                "body must name a button: up down left right a b x y l1 r1 select start guide");
        }
        if (!parseButton(name, button)) {
            return refuse("unknown button \"" + std::string{name} + "\"");
        }
        target_.inject(button);
        lucent::info("control", "injected {}", name);
        return lucent::http::Response::json(200, "OK", "{\"injected\":" + jsonString(name) + "}");
    }

    if (path == "/frame.png") {
        if (request.method != "GET") {
            return refuse("GET only");
        }
        std::string png;
        if (!target_.captureFrame(png)) {
            return serverError("could not render a frame");
        }
        return lucent::http::Response::binary(200, "OK", "image/png", std::move(png));
    }

    if (path == "/quit") {
        if (request.method != "POST") {
            return refuse("POST only");
        }
        target_.requestClose();
        return lucent::http::Response::json(200, "OK", "{\"closing\":true}");
    }

    return notFound("no route " + std::string{path});
}

} // namespace iideck::app