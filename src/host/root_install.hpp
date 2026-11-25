// root_install — something openSU puts in place as root in one step: its files are put together,
// owner only, in a stage directory of its own under the runtime dir, and the installer among them
// runs there through polkit's pkexec, which asks openSU's own agent (`polkit_agent`) for the
// password. What came of it is told apart: installed, cancelled at authentication, or failed with
// the installer's or polkit's last line. Session mode and the stick lights' helper install this way.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

/// pkexec's exit status when the player dismissed the password dialog.
inline constexpr int pkexecDismissed = 126;
/// The first line every installer prints, which tells openSU that polkit let it run.
inline constexpr const char* installerMarker = "opensu-install: start";

/// The root step as it runs: the program (pkexec) and its arguments, over the staged files.
struct RootStep {
    std::string program;
    std::vector<std::string> args;

    /// The command line, as a player would type it.
    [[nodiscard]] std::string shown() const;
};

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

/// One file of a root step and its contents.
struct StagedFile {
    std::string name;
    std::string bytes;
};

/// What one root step installs, from where.
struct RootStepPlan {
    /// The log's tag and what is installed, as messages name it ("session mode").
    std::string tag;
    std::string what;
    /// Where the files are put together, and polkit's pkexec (empty when there is none).
    std::filesystem::path stage;
    std::filesystem::path pkexec;
    /// The files, the installer among them by `installer`.
    std::vector<StagedFile> files;
    std::string installer;
    /// What pkexec runs in place of the installer, for a test run; empty for the installer.
    std::filesystem::path standIn;
};

/// Why `plan` cannot run here (no pkexec, with how to get it), else empty.
[[nodiscard]] std::string rootStepBlocker(const RootStepPlan& plan);

/// Puts the plan's files in its stage (owner only) and says how the root step runs over them.
/// Throws std::runtime_error when a file cannot be written.
[[nodiscard]] RootStep stageRootStep(const RootStepPlan& plan);

/// Stages and runs the plan through `run`; `inPlace` says afterwards whether it landed.
[[nodiscard]] InstallResult runRootStep(const Runner& run, const RootStepPlan& plan,
                                        const std::function<bool()>& inPlace);

/// Something installed by one root step.
class RootInstall {
  public:
    virtual ~RootInstall() = default;

    /// Whether it is in place.
    [[nodiscard]] virtual bool installed() const = 0;
    /// Empty when `install` can run here, else why not.
    [[nodiscard]] virtual std::string installBlocker() const = 0;
    /// Stages the root step's files and says how it would run them.
    [[nodiscard]] virtual RootStep stage() = 0;
    /// Stages the files and runs the root step through pkexec, which asks for the password.
    virtual InstallResult install() = 0;
};

} // namespace opensu::host
