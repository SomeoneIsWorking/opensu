// downloads — Steam's download queue, as its own client reports it live.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "devtools.hpp"

namespace opensu::steam {

/// One app with an install or update Steam has not finished.
struct Download {
    std::string appId;
    /// 0 to 1.
    double progress{0.0};
    /// Steam is transferring it now.
    bool active{false};
    bool paused{false};
    /// The active download is a first install rather than an update.
    bool installing{false};
    /// Seconds Steam expects the active download to take; nothing when it cannot say.
    std::optional<int> secondsLeft;
};

/// The unfinished downloads in Steam's answer to the query DownloadQueue sends: `items` from
/// RegisterForDownloadItems and `overview` from RegisterForDownloadOverview. The active one takes
/// its live figures from the overview.
[[nodiscard]] std::vector<Download> parseDownloads(const nlohmann::json& answer);

class DownloadQueue {
  public:
    explicit DownloadQueue(DevTools& devTools);

    /// The queue now; nothing when Steam cannot be asked, and `error` says why.
    [[nodiscard]] std::optional<std::vector<Download>> read(std::string& error);

  private:
    DevTools& devTools_;
};

} // namespace opensu::steam
