// download_control — changes to Steam's download queue through `SteamClient.Downloads`, the calls
// Steam's own downloads page makes (`steamui/chunk~2dcc5aaf7.js`): its pause and resume button
// (`EnableAllDownloads`, the whole queue), Download / Move to top on a waiting item
// (`ResumeAppUpdate`), a drag within the queue (`SetQueueIndex`) and the remove button
// (`RemoveFromDownloadList`). Each names this machine's client, id "0".
#pragma once

#include <string>

#include "devtools.hpp"
#include "downloads.hpp"

namespace opensu::steam {

class DownloadControl {
  public:
    explicit DownloadControl(DevTools& devTools) : devTools_{devTools} {
    }

    /// Pauses the queue, as the active item's pause button does. Each returns empty on success,
    /// else why not.
    [[nodiscard]] std::string pause();
    /// Resumes the queue.
    [[nodiscard]] std::string resume();
    /// Starts `appId` now, ahead of the rest of the queue.
    [[nodiscard]] std::string moveToTop(const std::string& appId);
    /// Puts `appId` at `index` of the queue; the active download is index 0.
    [[nodiscard]] std::string setQueueIndex(const std::string& appId, int index);
    /// Takes `download` off the queue; removing the paused head of the queue resumes the rest, as
    /// Steam's own page does.
    [[nodiscard]] std::string remove(const Download& download);

  private:
    [[nodiscard]] std::string call(const std::string& expression, const std::string& what);

    DevTools& devTools_;
};

} // namespace opensu::steam
