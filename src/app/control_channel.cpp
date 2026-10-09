#include "control_channel.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string_view>

#include "lucent/log.h"

namespace opensu::app {
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

/// `text` without the whitespace around it.
std::string_view trimmed(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    return text;
}

/// Whether `text` can be a store's authorization code: a short token of letters, digits and
/// `-_.~`, so it can never be taken for an option when it is handed to another program.
bool isAuthorizationCode(std::string_view text) {
    constexpr std::size_t longestCode = 512;
    if (text.empty() || text.size() > longestCode || text.front() == '-') {
        return false;
    }
    return std::ranges::all_of(text, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' ||
               c == '.' || c == '~';
    });
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
    text("section", snapshot.section);
    text("libraryMode", snapshot.libraryMode);
    flag("modeChooserOpen", snapshot.modeChooserOpen);
    flag("searchOpen", snapshot.searchOpen);
    text("searchText", snapshot.searchText);
    flag("contextMenuOpen", snapshot.contextMenuOpen);
    number("iconSize", snapshot.iconSize);
    text("shelf", snapshot.shelf);
    text("status", snapshot.status);
    text("toast", snapshot.toast);
    flag("toastIsError", snapshot.toastIsError);
    flag("launching", snapshot.launching);
    flag("inGame", snapshot.inGame);
    flag("gameMenuOpen", snapshot.gameMenuOpen);
    text("steam", snapshot.steam);
    text("launchers", snapshot.launchers);
    text("inputDevice", snapshot.inputDevice, true);
    return out;
}

} // namespace

bool parseButton(std::string_view name, gamepad::Button& out) {
    // The same spellings the shell's own hints use, so a caller driving the
    // shell by HTTP and a person reading a footer agree on what "a" means.
    static constexpr std::array<std::pair<std::string_view, gamepad::Button>, 14> kNames{{
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
        {"search", gamepad::Button::Search},
    }};
    for (const auto& [spelling, button] : kNames) {
        if (spelling == name) {
            out = button;
            return true;
        }
    }
    return false;
}

ControlChannel::ControlChannel(ControlTarget& target, SignInService& signIn, std::uint16_t port)
    : target_{target}, signIn_{signIn},
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
        std::string_view name = trimmed(request.body);
        // An optional second word says the press came from the keyboard: `a keyboard`.
        input::Device device = input::Device::Pad;
        if (const std::size_t space = name.find_first_of(" \t"); space != std::string_view::npos) {
            if (trimmed(name.substr(space)) != "keyboard") {
                return refuse("the only word after a button is \"keyboard\"");
            }
            device = input::Device::KeyboardMouse;
            name = name.substr(0, space);
        }
        if (name.empty()) {
            return refuse("body must name a button: up down left right a b x y l1 r1 select start "
                          "guide search");
        }
        if (!parseButton(name, button)) {
            return refuse("unknown button \"" + std::string{name} + "\"");
        }
        target_.inject(button, device);
        lucent::info("control", "injected {}", name);
        return lucent::http::Response::json(200, "OK", "{\"injected\":" + jsonString(name) + "}");
    }

    if (path == "/text") {
        if (request.method != "POST") {
            return refuse("POST only");
        }
        // What a physical keyboard would type, for the search field.
        target_.typeText(request.body);
        return lucent::http::Response::json(200, "OK",
                                            "{\"typed\":" + jsonString(request.body) + "}");
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

    if (path.starts_with("/signin/")) {
        return signIn(request);
    }

    return notFound("no route " + std::string{path});
}

lucent::http::Response ControlChannel::signIn(const lucent::http::Request& request) {
    if (request.method != "POST") {
        return refuse("POST only");
    }
    std::string_view route = request.path().substr(std::string_view{"/signin/"}.size());
    const bool start = route.ends_with("/start");
    if (start) {
        route.remove_suffix(std::string_view{"/start"}.size());
    }
    Store store{};
    if (route == "gog") {
        store = Store::Gog;
    } else if (route == "epic") {
        store = Store::Epic;
    } else {
        return notFound("no store " + std::string{route});
    }
    const std::string label = store == Store::Gog ? "GOG" : "Epic";

    SignInResult result;
    if (start) {
        result = signIn_.open(store);
    } else {
        const std::string_view code = trimmed(request.body);
        if (!isAuthorizationCode(code)) {
            return refuse("body must be the authorization code");
        }
        result = signIn_.complete(store, code);
        if (result.ok) {
            target_.requestCatalogReload(result.message);
        }
    }
    return lucent::http::Response::json(result.ok ? 200 : 502, result.ok ? "OK" : "Bad Gateway",
                                        "{\"ok\":" + std::string{result.ok ? "true" : "false"} +
                                            ",\"store\":" + jsonString(label) +
                                            ",\"message\":" + jsonString(result.message) + "}");
}

} // namespace opensu::app