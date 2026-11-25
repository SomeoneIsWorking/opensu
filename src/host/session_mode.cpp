#include "session_mode.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "lucent/log.h"
#include "session_scripts.hpp"

namespace opensu::host {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view sessionArgument = " --session";
/// The characters a desktop entry's Exec value must quote, and those escaped inside the quotes.
constexpr std::string_view execPlain =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./+-:,@%";
constexpr std::string_view execEscaped = "\"`$\\";

/// `program` as the first word of an Exec value, quoted when it has to be.
std::string execWord(const std::string& program) {
    if (!program.empty() && program.find_first_not_of(execPlain) == std::string::npos) {
        return program;
    }
    std::string word = "\"";
    for (const char c : program) {
        if (execEscaped.find(c) != std::string_view::npos) {
            // The Exec rule's backslash, itself escaped as a desktop entry string.
            word += "\\\\";
        }
        word += c == '\\' ? std::string{"\\\\"} : std::string{c};
    }
    return word + "\"";
}

/// The program an Exec value starts, undoing `execWord`; the rest after it in `rest`.
std::string execProgram(std::string_view command, std::string_view& rest) {
    if (!command.starts_with('"')) {
        const std::size_t space = command.find(' ');
        rest = space == std::string_view::npos ? std::string_view{} : command.substr(space);
        return std::string{command.substr(0, space)};
    }
    std::string program;
    for (std::size_t i = 1; i < command.size(); ++i) {
        if (command.substr(i).starts_with("\\\\") && i + 2 < command.size()) {
            const char next = command[i + 2];
            if (next == '\\' && command.substr(i + 2).starts_with("\\\\")) {
                program += '\\';
                i += 3;
                continue;
            }
            program += next;
            i += 2;
            continue;
        }
        if (command[i] == '"') {
            rest = command.substr(i + 1);
            return program;
        }
        program += command[i];
    }
    rest = {};
    return {};
}

/// The program a session entry starts with `--session`; empty when the file has none or is not
/// for a session.
std::string sessionProgram(const fs::path& entry) {
    std::ifstream in{entry};
    std::string line;
    constexpr std::string_view key = "Exec=";
    while (std::getline(in, line)) {
        if (!line.starts_with(key)) {
            continue;
        }
        std::string_view rest;
        std::string program = execProgram(std::string_view{line}.substr(key.size()), rest);
        return rest == sessionArgument ? program : std::string{};
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
    SddmSessionMode(Runner run, SessionPaths paths)
        : run_{std::move(run)}, paths_{std::move(paths)} {
    }

    bool installed() const override {
        std::error_code error;
        if (!fs::is_regular_file(paths_.selector, error)) {
            return false;
        }
        for (const fs::path& dir : paths_.entryDirs) {
            if (samePath(sessionProgram(dir / sessionEntryName), paths_.program)) {
                return true;
            }
        }
        return false;
    }

    std::string installBlocker() const override {
        if (paths_.program.empty()) {
            return "openSU cannot tell where it runs from, so no session can start it";
        }
        return rootStepBlocker(installPlan(paths_));
    }

    RootStep stage() override {
        return stageRootStep(installPlan(paths_));
    }

    InstallResult install() override {
        if (paths_.program.empty()) {
            return {InstallResult::Kind::Failed, installBlocker()};
        }
        return runRootStep(run_, installPlan(paths_), [this] {
            return installed();
        });
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
        }
        // The note marks this end as deliberate, so the session neither restores nor starts again.
        if (const std::string failure = paths_.request.write(); !failure.empty()) {
            return failure;
        }
        lucent::info("session", "ending openSU's session by stopping {}", paths_.compositorScope);
        return refusalOf("systemctl", run_("systemctl", {"--user", "stop", paths_.compositorScope}),
                         "end the session");
    }

  private:
    std::string select(const char* argument) {
        return runSelector(run_, paths_.selector, argument);
    }

    Runner run_;
    SessionPaths paths_;
};

class ReadOnlySessionMode final : public SessionMode {
  public:
    ReadOnlySessionMode(std::unique_ptr<SessionMode> inner, std::string reason, bool standInStep)
        : inner_{std::move(inner)}, reason_{std::move(reason)}, standInStep_{standInStep} {
    }

    bool installed() const override {
        return inner_->installed();
    }
    std::string installBlocker() const override {
        return inner_->installBlocker();
    }
    RootStep stage() override {
        return inner_->stage();
    }
    InstallResult install() override {
        if (standInStep_) {
            return inner_->install();
        }
        if (const std::string blocked = installBlocker(); !blocked.empty()) {
            return {InstallResult::Kind::Failed, blocked};
        }
        try {
            lucent::info("session", "not installing session mode ({}); it would run: {}", reason_,
                         stage().shown());
        } catch (const std::runtime_error& error) {
            lucent::warn("session", "staging session mode failed: {}", error.what());
        }
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
    bool standInStep_;
};

} // namespace

std::string sessionEntry(const fs::path& program) {
    return "[Desktop Entry]\n"
           "Type=Application\n"
           "Name=openSU\n"
           "Comment=Gamepad-first game shell as its own session on Gamescope\n"
           "Exec=" +
           execWord(program.string()) + std::string{sessionArgument} +
           "\n"
           "DesktopNames=gamescope\n";
}

RootStepPlan installPlan(const SessionPaths& paths) {
    return RootStepPlan{
        .tag = "session",
        .what = "session mode",
        .stage = paths.stage,
        .pkexec = paths.pkexec,
        .files = {{installerName, std::string{installSessionScript}},
                  {stagedSelectorName, std::string{sessionSelectScript}},
                  {sessionEntryName, sessionEntry(paths.program)}},
        .installer = installerName,
        .standIn = paths.rootStepStandIn};
}

std::string runSelector(const Runner& run, const fs::path& selector, const char* argument) {
    return refusalOf("sudo", run("sudo", {"-n", selector.string(), argument}),
                     std::string{"select "} + argument);
}

std::unique_ptr<SessionMode> makeSddmSessionMode(Runner run, SessionPaths paths) {
    return std::make_unique<SddmSessionMode>(std::move(run), std::move(paths));
}

std::unique_ptr<SessionMode> makeReadOnlySessionMode(std::unique_ptr<SessionMode> inner,
                                                     std::string reason, bool standInStep) {
    return std::make_unique<ReadOnlySessionMode>(std::move(inner), std::move(reason), standInStep);
}

} // namespace opensu::host
