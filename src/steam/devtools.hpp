// devtools — runs JavaScript in the Steam client's SharedJSContext, the page that holds its
// SteamClient API, over Chrome DevTools. Steam serves DevTools only when started with
// -cef-enable-debugging.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "lucent/http_client.h"

namespace iideck::steam {

/// Steam's DevTools port; the client has no switch to move it.
inline constexpr std::uint16_t devToolsPort = 8080;

class DevTools {
  public:
    explicit DevTools(std::uint16_t port = devToolsPort);

    /// Evaluates `expression`, awaiting it when it is a promise, and returns its value. Nothing
    /// when Steam cannot be reached or the script throws; `error` says which. Callable from any
    /// thread; calls are serialised on one connection, reopened when it drops.
    [[nodiscard]] std::optional<nlohmann::json> evaluate(std::string_view expression,
                                                         std::string& error);

  private:
    bool connect(std::string& error);

    std::uint16_t port_;
    std::mutex mutex_;
    lucent::http::WebSocketClient socket_;
    std::uint64_t nextId_{1};
};

} // namespace iideck::steam
