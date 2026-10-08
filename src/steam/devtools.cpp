#include "devtools.hpp"

#include <string>

namespace iideck::steam {
namespace {

using nlohmann::json;

/// The page whose window object carries SteamClient.
constexpr std::string_view sharedContextTitle = "SharedJSContext";
constexpr std::string_view devToolsPrefix = "ws://127.0.0.1:";

} // namespace

DevTools::DevTools(std::uint16_t port) : port_{port} {
}

bool DevTools::connect(std::string& error) {
    lucent::http::FetchResult targets;
    if (!lucent::http::get_loopback(port_, "/json", targets, error)) {
        error = "Steam DevTools unreachable: " + error;
        return false;
    }
    const json list = json::parse(targets.body, nullptr, false);
    if (targets.status != 200 || !list.is_array()) {
        error = "Steam DevTools listed no pages";
        return false;
    }
    for (const json& target : list) {
        if (target.value("title", "") != sharedContextTitle) {
            continue;
        }
        const std::string url = target.value("webSocketDebuggerUrl", "");
        const std::string prefix = std::string{devToolsPrefix} + std::to_string(port_);
        if (!url.starts_with(prefix)) {
            error = "Steam's SharedJSContext is not on loopback: " + url;
            return false;
        }
        return socket_.connect_loopback(port_, url.substr(prefix.size()), error);
    }
    error = "Steam has no SharedJSContext yet";
    return false;
}

std::optional<json> DevTools::evaluate(std::string_view expression, std::string& error) {
    const std::lock_guard lock{mutex_};
    if (!socket_.connected() && !connect(error)) {
        return std::nullopt;
    }
    const std::uint64_t id = nextId_++;
    const json request{
        {"id", id},
        {"method", "Runtime.evaluate"},
        {"params", {{"expression", expression}, {"awaitPromise", true}, {"returnByValue", true}}}};
    if (!socket_.send_text(request.dump(), error)) {
        return std::nullopt;
    }
    for (;;) {
        const std::optional<std::string> message = socket_.receive_text(error);
        if (!message) {
            return std::nullopt;
        }
        const json reply = json::parse(*message, nullptr, false);
        // Events and stale replies share the socket; only this call's reply answers it.
        if (!reply.is_object() || reply.value("id", std::uint64_t{0}) != id) {
            continue;
        }
        if (reply.contains("error")) {
            error = reply["error"].value("message", "DevTools refused the call");
            return std::nullopt;
        }
        const json& result = reply["result"];
        if (result.contains("exceptionDetails")) {
            const json& details = result["exceptionDetails"];
            error = details.contains("exception")
                        ? details["exception"].value("description", "script threw")
                        : details.value("text", "script threw");
            return std::nullopt;
        }
        return result["result"].value("value", json{});
    }
}

} // namespace iideck::steam
