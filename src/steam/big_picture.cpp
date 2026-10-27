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

bool BigPicture::show(const std::stop_token& stop, std::chrono::steady_clock::time_point deadline,
                      std::chrono::milliseconds poll, bool& opened, std::string& error) {
    std::optional<bool> up = shown(error);
    if (!up) {
        return false;
    }
    if (*up) {
        return true;
    }
    if (!open(error)) {
        return false;
    }
    opened = true;
    while (rest(stop, poll)) {
        if (std::chrono::steady_clock::now() > deadline) {
            error = "Steam's Big Picture did not open";
            return false;
        }
        up = shown(error);
        if (!up) {
            return false;
        }
        if (*up) {
            return true;
        }
    }
    error = "cancelled";
    return false;
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

bool openInSteamBrowser(DevTools& devTools, const std::string& url, std::string& error) {
    const std::string target = nlohmann::json("steam://openurl/" + url).dump();
    return devTools.evaluate("SteamClient.URL.ExecuteSteamURL(" + target + "), true", error)
        .has_value();
}

} // namespace opensu::steam
