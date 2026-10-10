#include "session_mode.hpp"

#include <fstream>
#include <utility>

#include "lucent/log.h"

namespace opensu::host {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view sessionArgument = " --session";
constexpr const char* installerRelative = "libexec/opensu/install-session.sh";

/// The `Exec=` command of a session entry, without its `--session` argument; empty when the file
/// has none or is not for a session.
std::string sessionProgram(const fs::path& entry) {
    std::ifstream in{entry};
    std::string line;
    constexpr std::string_view key = "Exec=";
    while (std::getline(in, line)) {
        if (!line.starts_with(key)) {
            continue;
        }
        const std::string_view command = std::string_view{line}.substr(key.size());
        if (command.ends_with(sessionArgument)) {
            return std::string{command.substr(0, command.size() - sessionArgument.size())};
        }
        return {};
    }
    return {};
}

bool samePath(const fs::path& a, const fs::path& b) {
    std::error_code error;
    return !a.empty() && !b.empty() &&
           fs::weakly_canonical(a, error) == fs::weakly_canonical(b, error);
}

class SddmSessionMode final : public SessionMode {
  public:
    SddmSessionMode(Runner run, LineRunner runLine, SessionPaths paths)
        : run_{std::move(run)}, runLine_{std::move(runLine)}, paths_{std::move(paths)} {
    }

    bool installed() const override {
        std::error_code error;
        if (!fs::is_regular_file(paths_.selector, error)) {
            return false;
        }
        for (const fs::path& dir : paths_.entryDirs) {
            if (samePath(sessionProgram(dir / sessionEntryName), paths_.executable)) {
                return true;
            }
        }
        return false;
    }

    std::string installBlocker() const override {
        std::error_code error;
        if (paths_.installer.empty() || !fs::is_regular_file(paths_.installer, error)) {
            return "Needs an installed openSU: run cmake --install";
        }
        return {};
    }

    InstallResult install(std::string_view password) override {
        lucent::info("session", "installing session mode with {}", paths_.installer.string());
        // sudo reads the password from stdin (-S) and never caches it (-k); the empty prompt keeps
        // sudo's own text out of the output.
        const std::optional<launch::Captured> out = runLine_(
            "sudo", {"-S", "-k", "-p", "", "/bin/sh", paths_.installer.string()}, password);
        if (!out) {
            return {InstallResult::Kind::Failed, "sudo could not be run"};
        }
        if (out->status == 0) {
            lucent::info("session", "session mode installed");
            return {};
        }
        const std::string reason = refusalOf("sudo", out, "install session mode");
        // The installer's first line says sudo accepted the password; without it sudo refused.
        const bool began = out->output.find(installerMarker) != std::string::npos;
        lucent::warn("session", "installing session mode failed: {}", reason);
        return {began ? InstallResult::Kind::Failed : InstallResult::Kind::Refused,
                began ? lastLine(out->output) : reason};
    }

    std::string switchToSession() override {
        if (const std::string failure = select("opensu"); !failure.empty()) {
            return failure;
        }
        lucent::info("session", "ending the desktop session for openSU");
        const std::string failure =
            refusalOf("busctl",
                      run_("busctl", {"--user", "call", "org.kde.Shutdown", "/Shutdown",
                                      "org.kde.Shutdown", "logout"}),
                      "log out");
        if (!failure.empty()) {
            static_cast<void>(select("restore"));
        }
        return failure;
    }

    std::string switchToDesktop() override {
        std::error_code error;
        if (fs::is_regular_file(paths_.selector, error)) {
            if (const std::string failure = select("restore"); !failure.empty()) {
                return failure;
            }
        } else {
            // No selector: the session starts the desktop itself once Gamescope has ended.
            lucent::info("session", "no {}; the session will start the desktop itself",
                         paths_.selector.string());
            if (const std::string failure = paths_.request.write(); !failure.empty()) {
                return failure;
            }
        }
        lucent::info("session", "ending openSU's session by stopping {}", paths_.compositorScope);
        return refusalOf("systemctl", run_("systemctl", {"--user", "stop", paths_.compositorScope}),
                         "end the session");
    }

  private:
    /// Runs the selector with a fixed `argument` through `sudo -n`, which never asks for a
    /// password.
    std::string select(const char* argument) {
        return refusalOf("sudo", run_("sudo", {"-n", paths_.selector.string(), argument}),
                         std::string{"select "} + argument);
    }

    static std::string lastLine(const std::string& output) {
        const std::size_t end = output.find_last_not_of('\n');
        if (end == std::string::npos) {
            return {};
        }
        const std::size_t newline = output.find_last_of('\n', end);
        const std::size_t start = newline == std::string::npos ? 0 : newline + 1;
        return output.substr(start, end - start + 1);
    }

    Runner run_;
    LineRunner runLine_;
    SessionPaths paths_;
};

class ReadOnlySessionMode final : public SessionMode {
  public:
    ReadOnlySessionMode(std::unique_ptr<SessionMode> inner, std::string reason)
        : inner_{std::move(inner)}, reason_{std::move(reason)} {
    }

    bool installed() const override {
        return inner_->installed();
    }
    std::string installBlocker() const override {
        return inner_->installBlocker();
    }
    InstallResult install(std::string_view) override {
        return {InstallResult::Kind::Failed, reason_};
    }
    std::string switchToSession() override {
        return reason_;
    }
    std::string switchToDesktop() override {
        return reason_;
    }

  private:
    std::unique_ptr<SessionMode> inner_;
    std::string reason_;
};

} // namespace

SessionPaths SessionPaths::forExecutable(const fs::path& executable, DesktopRequest request,
                                         std::string compositorScope) {
    SessionPaths paths;
    paths.executable = executable;
    if (!executable.empty()) {
        paths.installer = executable.parent_path().parent_path() / installerRelative;
    }
    paths.request = std::move(request);
    paths.compositorScope = std::move(compositorScope);
    return paths;
}

std::unique_ptr<SessionMode> makeSddmSessionMode(Runner run, LineRunner runLine,
                                                 SessionPaths paths) {
    return std::make_unique<SddmSessionMode>(std::move(run), std::move(runLine), std::move(paths));
}

std::unique_ptr<SessionMode> makeReadOnlySessionMode(std::unique_ptr<SessionMode> inner,
                                                     std::string reason) {
    return std::make_unique<ReadOnlySessionMode>(std::move(inner), std::move(reason));
}

} // namespace opensu::host
