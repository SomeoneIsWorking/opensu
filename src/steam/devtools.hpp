// devtools — runs JavaScript in a page of the Steam client over Chrome DevTools: its
// SharedJSContext, the page that holds its SteamClient API, or one browser view by its target
// id. Also lists Steam's pages and closes one. Steam serves DevTools only when started with
// -cef-enable-debugging.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "lucent/http_client.h"

namespace opensu::steam {

/// Steam's DevTools port; the client has no switch to move it.
inline constexpr std::uint16_t devToolsPort = 8080;

/// A page DevTools lists (`/json`): Steam's own windows and each browser view.
struct DevToolsTarget {
    std::string id;
    /// "page" for a window or browser view.
    std::string type;
    std::string title;
    std::string url;
};

/// Every page Steam's DevTools on `port` lists; nothing when it cannot be reached.
[[nodiscard]] std::optional<std::vector<DevToolsTarget>> devToolsTargets(std::uint16_t port,
                                                                         std::string& error);

/// Closes the page `id` (`/json/close/<id>`), as its own window's close button does.
[[nodiscard]] bool closeDevToolsTarget(std::uint16_t port, std::string_view id, std::string& error);

class DevTools {
  public:
    /// Steam's SharedJSContext.
    explicit DevTools(std::uint16_t port = devToolsPort);
    /// The page with target id `targetId`.
    DevTools(std::uint16_t port, std::string targetId);

    /// Evaluates `expression`, awaiting it when it is a promise, and returns its value. Nothing
    /// when Steam cannot be reached or the script throws; `error` says which. Callable from any
    /// thread; calls are serialised on one connection, reopened when it drops.
    [[nodiscard]] std::optional<nlohmann::json> evaluate(std::string_view expression,
                                                         std::string& error);

  private:
    bool connect(std::string& error);
    [[nodiscard]] bool isTarget(const nlohmann::json& target) const;

    std::uint16_t port_;
    /// Empty for the SharedJSContext.
    std::string targetId_;
    std::mutex mutex_;
    lucent::http::WebSocketClient socket_;
    std::uint64_t nextId_{1};
};

} // namespace opensu::steam
