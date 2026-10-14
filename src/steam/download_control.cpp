#include "download_control.hpp"

#include "lucent/log.h"
#include "steam_json.hpp"

namespace opensu::steam {
namespace {

/// This machine's client; remote clients have their own ids.
constexpr std::string_view localClient = R"("0")";

/// An app id as a JavaScript number, or empty when it is not one.
std::string appNumber(const std::string& appId) {
    return isAppId(appId) ? appId : std::string{};
}

} // namespace

std::string DownloadControl::call(const std::string& expression, const std::string& what) {
    std::string error;
    if (!devTools_.evaluate("SteamClient.Downloads." + expression, error)) {
        return "Steam would not " + what + ": " + error;
    }
    lucent::info("steam", "downloads: {}", what);
    return {};
}

std::string DownloadControl::pause() {
    return call("EnableAllDownloads(false, " + std::string{localClient} + ")", "pause downloads");
}

std::string DownloadControl::resume() {
    return call("EnableAllDownloads(true, " + std::string{localClient} + ")", "resume downloads");
}

std::string DownloadControl::moveToTop(const std::string& appId) {
    const std::string app = appNumber(appId);
    if (app.empty()) {
        return "not a Steam app id: " + appId;
    }
    return call("ResumeAppUpdate(" + app + ", " + std::string{localClient} + ")",
                "start " + app + " now");
}

std::string DownloadControl::setQueueIndex(const std::string& appId, int index) {
    const std::string app = appNumber(appId);
    if (app.empty()) {
        return "not a Steam app id: " + appId;
    }
    if (index < 0) {
        return "no queue place " + std::to_string(index);
    }
    return call("SetQueueIndex(" + app + ", " + std::to_string(index) + ", " +
                    std::string{localClient} + ")",
                "move " + app + " to place " + std::to_string(index));
}

std::string DownloadControl::remove(const Download& download) {
    const std::string app = appNumber(download.appId);
    if (app.empty()) {
        return "not a Steam app id: " + download.appId;
    }
    if (std::string refused =
            call("RemoveFromDownloadList(" + app + ", " + std::string{localClient} + ")",
                 "remove " + app + " from the queue");
        !refused.empty()) {
        return refused;
    }
    if (download.queueIndex == 0 && download.paused) {
        return resume();
    }
    return {};
}

} // namespace opensu::steam
