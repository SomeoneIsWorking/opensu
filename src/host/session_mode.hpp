// session_mode — openSU as a login session you can switch to and back from.
//
// Installing it is one root step (`install-session.sh`, run through a single `sudo -S`) that puts
// the session entry where SDDM reads it, a root-owned selector at
// /usr/local/libexec/opensu-session-select and a sudoers rule that lets the user run the selector
// with `opensu` or `restore` and nothing else. Switching runs the selector through `sudo -n`, then
// ends the session: the desktop through KDE's logout call, openSU by stopping its Gamescope. SDDM
// then logs in to whatever the selector chose. Without the selector, leaving openSU falls back to
// a note and a desktop started in the session's place.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "desktop_request.hpp"
#include "runner.hpp"

namespace opensu::host {

/// The directories SDDM reads sessions from, in its order.
inline constexpr std::array<std::string_view, 2> sddmSessionDirs{
    "/usr/local/share/wayland-sessions", "/usr/share/wayland-sessions"};
/// The selector the installer puts in place, and what the sudoers rule names.
inline constexpr const char* selectorPath = "/usr/local/libexec/opensu-session-select";
/// What the installer's sudoers rule is called.
inline constexpr const char* sudoersRulePath = "/etc/sudoers.d/zz-opensu-session-select";
/// The session entry's file name, and the installer's first output line.
inline constexpr const char* sessionEntryName = "opensu.desktop";
inline constexpr const char* installerMarker = "opensu-install: start";

struct SessionPaths {
    /// Where an entry pointing at `executable` makes the session installed.
    std::vector<std::filesystem::path> entryDirs{sddmSessionDirs.begin(), sddmSessionDirs.end()};
    std::filesystem::path selector{selectorPath};
    /// `install-session.sh` beside the installed executable; empty when there is none.
    std::filesystem::path installer;
    /// This openSU, which the entry's Exec line must name.
    std::filesystem::path executable;
    /// The fallback's note, and the scope of the session's Gamescope, which ending is how openSU's
    /// session ends.
    DesktopRequest request{{}};
    std::string compositorScope;

    /// The paths for an openSU running as `executable`.
    [[nodiscard]] static SessionPaths forExecutable(const std::filesystem::path& executable,
                                                    DesktopRequest request,
                                                    std::string compositorScope);
};

/// How the root step went.
struct InstallResult {
    enum class Kind : std::uint8_t {
        Installed,
        /// sudo did not accept the password (or the user): ask again.
        Refused,
        /// The installer began and failed, or could not be started.
        Failed,
    };
    Kind kind{Kind::Installed};
    /// Empty when installed, else why not.
    std::string message;
};

class SessionMode {
  public:
    virtual ~SessionMode() = default;

    /// Whether a session entry for this openSU is where SDDM reads and the selector is in place.
    [[nodiscard]] virtual bool installed() const = 0;
    /// Empty when `install` can run from here, else why not (no installer beside this openSU).
    [[nodiscard]] virtual std::string installBlocker() const = 0;
    /// Runs the root step with `password` on sudo's stdin. The password is not kept or logged.
    virtual InstallResult install(std::string_view password) = 0;
    /// Makes openSU the next login and ends the desktop session. Empty when done, else why not;
    /// the selection is undone when the session cannot be ended.
    virtual std::string switchToSession() = 0;
    /// Makes the previous session the next login and ends openSU's session; empty when done.
    virtual std::string switchToDesktop() = 0;
};

/// Runs `selector` with `argument` (`opensu` or `restore`) through `sudo -n`, which never asks for
/// a password. Empty when it did, else why not.
[[nodiscard]] std::string runSelector(const Runner& run, const std::filesystem::path& selector,
                                      const char* argument);

/// SDDM and sudo over `run` and `runLine`.
[[nodiscard]] std::unique_ptr<SessionMode> makeSddmSessionMode(Runner run, LineRunner runLine,
                                                               SessionPaths paths);

/// `inner` for reading what is installed; every change refuses with `reason`. For hidden runs.
[[nodiscard]] std::unique_ptr<SessionMode>
makeReadOnlySessionMode(std::unique_ptr<SessionMode> inner, std::string reason);

} // namespace opensu::host
