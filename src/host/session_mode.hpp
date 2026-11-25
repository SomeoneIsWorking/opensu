// session_mode — openSU as a login session you can switch to and back from.
//
// Installing it is one root step. openSU puts together, in a directory of its own under the
// runtime dir, the installer and the selector it has built in (`session_scripts`) and a session
// entry that starts this openSU, and runs the installer there through polkit (`pkexec`), which asks
// openSU's own agent (`polkit_agent`) for the password. The installer puts the entry where SDDM
// reads it, a root-owned selector at /usr/local/libexec/opensu-session-select and a sudoers rule
// that lets the user run the selector with `opensu` or `restore` and nothing else. Switching runs
// the selector through `sudo -n`, then ends the session: the desktop through KDE's logout call,
// openSU by stopping its Gamescope. SDDM then logs in to whatever the selector chose. Without the
// selector, leaving openSU falls back to a note and a desktop started in the session's place.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "desktop_request.hpp"
#include "root_install.hpp"
#include "runner.hpp"

namespace opensu::host {

/// The directories SDDM reads sessions from, in its order.
inline constexpr std::array<std::string_view, 2> sddmSessionDirs{
    "/usr/local/share/wayland-sessions", "/usr/share/wayland-sessions"};
/// The selector the installer puts in place, and what the sudoers rule names.
inline constexpr const char* selectorPath = "/usr/local/libexec/opensu-session-select";
/// What the installer's sudoers rule is called.
inline constexpr const char* sudoersRulePath = "/etc/sudoers.d/zz-opensu-session-select";
/// The session entry's file name.
inline constexpr const char* sessionEntryName = "opensu.desktop";
/// The staged installer's and selector's file names.
inline constexpr const char* installerName = "install-session.sh";
inline constexpr const char* stagedSelectorName = "opensu-session-select";

struct SessionPaths {
    /// Where an entry starting `program` makes the session installed.
    std::vector<std::filesystem::path> entryDirs{sddmSessionDirs.begin(), sddmSessionDirs.end()};
    std::filesystem::path selector{selectorPath};
    /// What the session entry starts: the AppImage openSU runs from, else its executable.
    std::filesystem::path program;
    /// Where the root step's files are put together: `<runtime dir>/session-install`.
    std::filesystem::path stage;
    /// polkit's pkexec, as found on PATH; empty when there is none.
    std::filesystem::path pkexec;
    /// What pkexec runs in place of the installer, for a test run; empty for the installer.
    std::filesystem::path rootStepStandIn;
    /// The fallback's note, and the scope of the session's Gamescope, which ending is how openSU's
    /// session ends.
    DesktopRequest request{{}};
    std::string compositorScope;
};

/// The session entry for `program`: a Wayland session that starts it with `--session`.
[[nodiscard]] std::string sessionEntry(const std::filesystem::path& program);

/// The root step that installs the installer, the selector and the session entry for
/// `paths.program`, staged in `paths.stage`.
[[nodiscard]] RootStepPlan installPlan(const SessionPaths& paths);

/// Installed when a session entry for this openSU is where SDDM reads and the selector is in place.
class SessionMode : public RootInstall {
  public:
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

/// SDDM, polkit and sudo over `run`.
[[nodiscard]] std::unique_ptr<SessionMode> makeSddmSessionMode(Runner run, SessionPaths paths);

/// `inner` for reading what is installed and staging the root step; every change refuses with
/// `reason`, the install after logging the command it would have run, unless its root step is a
/// stand-in (`SessionPaths::rootStepStandIn`), which installs nothing and runs. For hidden runs.
[[nodiscard]] std::unique_ptr<SessionMode>
makeReadOnlySessionMode(std::unique_ptr<SessionMode> inner, std::string reason, bool standInStep);

} // namespace opensu::host
