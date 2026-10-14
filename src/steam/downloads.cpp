#include "downloads.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "steam_json.hpp"

namespace opensu::steam {
namespace {

using nlohmann::json;

// Both registrations call back at once with the current state.
constexpr std::string_view query = R"js(Promise.all([
  new Promise(done => { const h = SteamClient.Downloads.RegisterForDownloadItems((paused, items) => {
    h.unregister(); done(items); }); }),
  new Promise(done => { const h = SteamClient.Downloads.RegisterForDownloadOverview(overview => {
    h.unregister(); done(overview); }); })
]).then(([items, overview]) => ({ items, overview })))js";

/// The percentage of the item's first pending part, or nothing when it has none.
std::optional<int> pendingPercent(const json& item) {
    if (!item.contains("update_type_info")) {
        return std::nullopt;
    }
    for (const json& part : item["update_type_info"]) {
        if (part.value("has_update", false) && !part.value("completed_update", false)) {
            return part.value("overall_percent_complete", 0);
        }
    }
    return std::nullopt;
}

double fraction(int percent) {
    return std::clamp(percent, 0, 100) / 100.0;
}

/// k_EAppUpdateProgress_Download, the stage of an update's `progress` that counts the transfer.
constexpr std::size_t downloadStage = 2;

/// The transfer's bytes in `progress`, an update's stages.
std::pair<std::uint64_t, std::uint64_t> transferOf(const json& progress) {
    if (!progress.is_array() || progress.size() <= downloadStage) {
        return {0, 0};
    }
    const json& stage = progress[downloadStage];
    return {uint64Of(stage, "bytes_in_progress"), uint64Of(stage, "bytes_total")};
}

/// The bytes of every part of the item, as Steam's downloads page adds them up.
std::pair<std::uint64_t, std::uint64_t> itemBytes(const json& item) {
    std::pair<std::uint64_t, std::uint64_t> sum{0, 0};
    if (!item.contains("update_type_info") || !item["update_type_info"].is_array()) {
        return sum;
    }
    for (const json& part : item["update_type_info"]) {
        if (part.is_object() && part.contains("progress")) {
            const auto [done, total] = transferOf(part["progress"]);
            sum.first += done;
            sum.second += total;
        }
    }
    return sum;
}

} // namespace

std::vector<Download> parseDownloads(const json& answer) {
    std::vector<Download> downloads;
    if (!answer.is_object() || !answer.contains("items") || !answer["items"].is_array()) {
        return downloads;
    }
    const json overview = answer.value("overview", json::object());
    const std::int64_t activeId = overview.value("update_appid", std::int64_t{0});
    for (const json& client : answer["items"]) {
        // Downloads a remote Steam client runs show up here too; only this machine's count.
        if (client.value("remote_client_id", "0") != "0" || !client.contains("item_data")) {
            continue;
        }
        for (const json& item : client["item_data"]) {
            const std::optional<int> percent = pendingPercent(item);
            if (item.value("completed", false) || !percent) {
                continue;
            }
            const std::int64_t appId = item.value("appid", std::int64_t{0});
            const auto [done, total] = itemBytes(item);
            Download download{.appId = std::to_string(appId),
                              .progress = fraction(*percent),
                              .active = item.value("active", false),
                              .paused = item.value("paused", false),
                              .queueIndex = item.value("queue_index", -1),
                              .bytesDone = done,
                              .bytesTotal = total};
            const bool queuePaused = overview.value("paused", false);
            if (download.queueIndex == 0 && queuePaused) {
                download.paused = true;
            }
            if (appId == activeId && !queuePaused) {
                download.active = true;
                download.installing = overview.value("update_is_install", false);
                download.progress = fraction(overview.value("overall_percent_complete", 0));
                const int seconds = overview.value("overall_estimated_time_remaining_sec", 0);
                if (seconds > 0) {
                    download.secondsLeft = seconds;
                }
                if (const auto [liveDone, liveTotal] =
                        transferOf(overview.value("progress", json::array()));
                    liveTotal > 0) {
                    download.bytesDone = liveDone;
                    download.bytesTotal = liveTotal;
                }
                if (const std::uint64_t rate =
                        uint64Of(overview, "update_network_bytes_per_second");
                    rate > 0) {
                    download.bytesPerSecond = rate;
                }
                download.updateState = overview.value("update_state", "");
            }
            downloads.push_back(std::move(download));
        }
    }
    return downloads;
}

DownloadQueue::DownloadQueue(DevTools& devTools) : devTools_{devTools} {
}

std::optional<std::vector<Download>> DownloadQueue::read(std::string& error) {
    const std::optional<json> answer = devTools_.evaluate(query, error);
    if (!answer) {
        return std::nullopt;
    }
    return parseDownloads(*answer);
}

} // namespace opensu::steam
