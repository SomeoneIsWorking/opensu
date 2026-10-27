// big_picture — Steam's Big Picture through the running client: opened as `steam://open/bigpicture`
// opens it, told apart by `SteamClient.UI.GetUIMode()` (4 while it shows), and closed with
// `SteamClient.UI.ExitBigPictureMode()`, what Big Picture's own window calls on close
// (`steamui/chunk~2dcc5aaf7.js`). openSU is the controller-first screen; Big Picture is wanted only
// where openSU's own Steam has no other window to show a page in (the SteamGridDB sign-in).
// In Big Picture the SharedJSContext is its window, routed under `/routes/`; just after a logon it
// sits on `/routes/login` and then goes to its home over any page opened meanwhile, so it counts
// as up only once past that route.
#pragma once

#include <chrono>
#include <optional>
#include <stop_token>
#include <string>

#include "devtools.hpp"

namespace opensu::steam {

class BigPicture {
  public:
    explicit BigPicture(DevTools& devTools) : devTools_{devTools} {
    }

    /// Asks Steam to open Big Picture; false, with `error`, when Steam would not take the request.
    [[nodiscard]] bool open(std::string& error);
    /// Whether Big Picture shows and is past its login route; nothing, with `error`, when Steam
    /// cannot be asked.
    [[nodiscard]] std::optional<bool> shown(std::string& error);
    /// Shows Big Picture: opened when it does not show, then looked at every `poll` until it does.
    /// `opened` says whether this opened it. False with `error` when it is not up by `deadline`,
    /// Steam cannot be asked, or on `stop` ("cancelled").
    [[nodiscard]] bool show(const std::stop_token& stop,
                            std::chrono::steady_clock::time_point deadline,
                            std::chrono::milliseconds poll, bool& opened, std::string& error);
    /// Asks Steam to leave Big Picture; false, logged, when Steam would not take the request.
    bool close();

    /// Closes Big Picture on the way out when `opened` says this opened it.
    struct Leave {
        explicit Leave(BigPicture& shown) : bigPicture{shown} {
        }
        Leave(const Leave&) = delete;
        Leave(Leave&&) = delete;
        Leave& operator=(const Leave&) = delete;
        Leave& operator=(Leave&&) = delete;
        ~Leave() {
            if (opened) {
                bigPicture.close();
            }
        }
        BigPicture& bigPicture;
        bool opened{false};
    };

  private:
    DevTools& devTools_;
};

/// Opens `url` in a view of Steam's browser (`steam://openurl`); false with `error` when Steam
/// would not take it.
[[nodiscard]] bool openInSteamBrowser(DevTools& devTools, const std::string& url,
                                      std::string& error);

} // namespace opensu::steam
