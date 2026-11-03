// session_mode — openSU as a login session you can switch to and back from.
//
// Installing it is one root step. openSU puts together, in a directory of its own under the
// runtime dir, the installer and the selector it has built in (`session_scripts`) and a session
// entry that starts this openSU, and runs the installer there through polkit (`pkexec`), which asks
// for the password in the desktop's own dialog. The installer puts the entry where SDDM reads it, a
// root-owned selector at /usr/local/libexec/opensu-session-select and a sudoers rule that lets the
// user run the selector with `opensu` or `restore` and nothing else. Switching runs the selector
// through `sudo -n`, then ends the session: the desktop through KDE's logout call, openSU by
// stopping its Gamescope. SDDM then logs in to whatever the selector chose. Without the selector,
// leaving openSU falls back to a note and a desktop started in the session's place.
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
/// The staged installer's and selector's file names.
inline constexpr const char* installerName = "install-session.sh";
inline constexpr const char* stagedSelectorName = "opensu-session-select";
/// pkexec's exit status when the player dismissed the password dialog.
inline constexpr int pkexecDismissed = 126;

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
    /// The fallback's note, and the scope of the session's Gamescope, which ending is how openSU's
    /// session ends.
    DesktopRequest request{{}};
    std::string compositorScope;
};

/// The root step as it runs: the program (pkexec) and its arguments, over the staged files.
struct RootStep {
    std::string program;
    std::vector<std::string> args;

    /// The command line, as a player would type it.
    [[nodiscard]] std::string shown() const;
};

/// The session entry for `program`: a Wayland session that starts it with `--session`.
[[nodiscard]] std::string sessionEntry(const std::filesystem::path& program);

/// Puts the installer, the selector and the session entry for `paths.program` in `paths.stage`
/// (owner only) and says how the root step runs over them. Throws std::runtime_error when a file
/// cannot be written.
[[nodiscard]] RootStep stageInstall(const SessionPaths& paths);

/// How the root step went.
struct InstallResult {
    enum class Kind : std::uint8_t {
        Installed,
        /// The player dismissed the password dialog.
        Cancelled,
        /// polkit refused, the installer failed, or nothing could be started.
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
    /// Empty when `install` can run here, else why not (no pkexec, with how to get it).
    [[nodiscard]] virtual std::string installBlocker() const = 0;
    /// Stages the root step's files and says how it would run them.
    [[nodiscard]] virtual RootStep stage() = 0;
    /// Stages the files and runs the root step through pkexec, which asks for the password.
    virtual InstallResult install() = 0;
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
/// `reason`, the install after logging the command it would have run. For hidden runs.
[[nodiscard]] std::unique_ptr<SessionMode>
makeReadOnlySessionMode(std::unique_ptr<SessionMode> inner, std::string reason);

} // namespace opensu::host
