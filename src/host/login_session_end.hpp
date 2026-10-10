// login_session_end — what happens to the next login when openSU's login session ends.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

#include "desktop_request.hpp"
#include "runner.hpp"

namespace opensu::host {

/// A session that ends sooner than this after it started has failed, whatever its exit status.
inline constexpr std::chrono::seconds quickEndWindow{30};

/// Judges the end of a `--session` run. SDDM with autologin on openSU logs in again whenever the
/// session ends, so an end nobody asked for must hand the next login back to the previous
/// session, or a failing Gamescope is a relogin loop.
class LoginSessionEnd {
  public:
    enum class Ending : std::uint8_t {
        /// Nobody asked, but it ended well and after `quickEndWindow`: the login is left alone.
        Ordinary,
        /// Switch to desktop (selector installed): the selector already chose the next login.
        SwitchedToDesktop,
        /// Switch to desktop without the selector: the caller starts the desktop in its place.
        StartDesktop,
        /// Abnormal end: the previous session is the next login.
        Restored,
        /// Abnormal end and the restore failed; `failure()` says why. The loop is not broken.
        RestoreFailed,
        /// Abnormal end, no selector: openSU never changed the autologin, so there is nothing to
        /// undo.
        NothingToRestore,
    };

    /// The clock starts here. `selector` is the installed session selector; `request` the note a
    /// deliberate Switch to desktop leaves.
    LoginSessionEnd(Runner run, std::filesystem::path selector, DesktopRequest request,
                    std::chrono::seconds quickWindow = quickEndWindow);

    /// Judges the end of the session, whose compositor exited with `status`; `stopRequested` when
    /// openSU stopped it on SIGINT/SIGTERM (logout, shutdown), which is no failure. Acts on it.
    [[nodiscard]] Ending settle(int status, bool stopRequested);

    /// Why the last restore failed.
    [[nodiscard]] const std::string& failure() const noexcept {
        return failure_;
    }

  private:
    Runner run_;
    std::filesystem::path selector_;
    DesktopRequest request_;
    std::chrono::seconds quickWindow_;
    std::chrono::steady_clock::time_point started_;
    std::string failure_;
};

} // namespace opensu::host
