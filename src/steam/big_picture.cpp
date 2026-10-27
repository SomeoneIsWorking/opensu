#include "big_picture.hpp"

#include <string_view>

#include "lucent/log.h"

namespace opensu::steam {
namespace {

/// UI mode 4 is Big Picture, the desktop client 7; `GetUIMode` answers with a promise.
constexpr std::string_view upScript = R"js((async () =>
  (await SteamClient.UI.GetUIMode()) === 4 && location.pathname.startsWith("/routes/") &&
  !location.pathname.startsWith("/routes/login"))())js";

} // namespace

bool BigPicture::open(std::string& error) {
    if (!devTools_.evaluate("SteamClient.URL.ExecuteSteamURL(\"steam://open/bigpicture\"), true",
                            error)) {
        return false;
    }
    lucent::info("steam", "opened Big Picture");
    return true;
}

std::optional<bool> BigPicture::shown(std::string& error) {
    const std::optional<nlohmann::json> up = devTools_.evaluate(upScript, error);
    if (!up) {
        return std::nullopt;
    }
    return up->is_boolean() && up->get<bool>();
}

bool BigPicture::close() {
    std::string error;
    if (!devTools_.evaluate("SteamClient.UI.ExitBigPictureMode()", error)) {
        lucent::warn("steam", "Steam would not leave Big Picture: {}", error);
        return false;
    }
    lucent::info("steam", "closed Big Picture");
    return true;
}

} // namespace opensu::steam
