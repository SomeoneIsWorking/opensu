#include "devtools.hpp"

#include <condition_variable>
#include <string>

namespace opensu::steam {
namespace {

using nlohmann::json;

/// The page whose window object carries SteamClient.
constexpr std::string_view sharedContextTitle = "SharedJSContext";
constexpr std::string_view devToolsPrefix = "ws://127.0.0.1:";

/// The `/json` page list on `port`; nothing when it cannot be read.
std::optional<json> pageList(std::uint16_t port, std::string& error) {
    lucent::http::FetchResult targets;
    if (!lucent::http::get_loopback(port, "/json", targets, error)) {
        error = "Steam DevTools unreachable: " + error;
        return std::nullopt;
    }
    json list = json::parse(targets.body, nullptr, false);
    if (targets.status != 200 || !list.is_array()) {
        error = "Steam DevTools listed no pages";
        return std::nullopt;
    }
    return list;
}

} // namespace

bool rest(const std::stop_token& stop, std::chrono::milliseconds poll) {
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock{mutex};
    static_cast<void>(wake.wait_for(lock, stop, poll, [] {
        return false;
    }));
    return !stop.stop_requested();
}

std::optional<std::vector<DevToolsTarget>> devToolsTargets(std::uint16_t port, std::string& error) {
    const std::optional<json> list = pageList(port, error);
    if (!list) {
        return std::nullopt;
    }
    std::vector<DevToolsTarget> targets;
    for (const json& target : *list) {
        if (target.is_object()) {
            targets.push_back(DevToolsTarget{target.value("id", ""), target.value("type", ""),
                                             target.value("title", ""), target.value("url", "")});
        }
    }
    return targets;
}

bool closeDevToolsTarget(std::uint16_t port, std::string_view id, std::string& error) {
    lucent::http::FetchResult answer;
    if (!lucent::http::get_loopback(port, "/json/close/" + std::string{id}, answer, error)) {
        error = "Steam DevTools unreachable: " + error;
        return false;
    }
    if (answer.status != 200) {
        error =
            "Steam DevTools did not close the page (HTTP " + std::to_string(answer.status) + ")";
        return false;
    }
    return true;
}

DevTools::DevTools(std::uint16_t port) : port_{port} {
}

DevTools::DevTools(std::uint16_t port, std::string targetId)
    : port_{port}, targetId_{std::move(targetId)} {
}

bool DevTools::isTarget(const json& target) const {
    return targetId_.empty() ? target.value("title", "") == sharedContextTitle
                             : target.value("id", "") == targetId_;
}

bool DevTools::connect(std::string& error) {
    const std::optional<json> list = pageList(port_, error);
    if (!list) {
        return false;
    }
    for (const json& target : *list) {
        if (!target.is_object() || !isTarget(target)) {
            continue;
        }
        const std::string url = target.value("webSocketDebuggerUrl", "");
        const std::string prefix = std::string{devToolsPrefix} + std::to_string(port_);
        if (!url.starts_with(prefix)) {
            error = "Steam's page is not on loopback: " + url;
            return false;
        }
        return socket_.connect_loopback(port_, url.substr(prefix.size()), error);
    }
    error = targetId_.empty() ? "Steam has no SharedJSContext yet" : "the page is closed";
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

} // namespace opensu::steam
