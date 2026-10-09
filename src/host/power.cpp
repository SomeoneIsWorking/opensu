#include "power.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "lucent/log.h"

namespace opensu::host {
namespace {

const char* verb(PowerAction action) noexcept {
    switch (action) {
    case PowerAction::Suspend:
        return "suspend";
    case PowerAction::Restart:
        return "reboot";
    case PowerAction::ShutDown:
        return "poweroff";
    case PowerAction::SwitchToDesktop:
        return "";
    }
    return "";
}

constexpr const char* sessionSelect = "steamos-session-select";

bool onPath(const std::vector<std::filesystem::path>& path, const char* program) {
    namespace fs = std::filesystem;
    constexpr fs::perms execute =
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
    return std::ranges::any_of(path, [program](const fs::path& directory) {
        std::error_code error;
        const fs::path candidate = directory / program;
        return fs::is_regular_file(candidate, error) &&
               (fs::status(candidate, error).permissions() & execute) != fs::perms::none;
    });
}

/// The first line of `output`, trimmed.
std::string firstLine(const std::string& output) {
    const auto isSpace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };
    const auto begin = std::find_if_not(output.begin(), output.end(), isSpace);
    const auto end = std::find(begin, output.end(), '\n');
    std::string line{begin, end};
    while (!line.empty() && isSpace(static_cast<unsigned char>(line.back()))) {
        line.pop_back();
    }
    return line;
}

class LogindPower final : public Power {
  public:
    LogindPower(Runner run, SessionExit exit) : run_{std::move(run)}, exit_{std::move(exit)} {
    }

    std::string perform(PowerAction action) override {
        if (action == PowerAction::SwitchToDesktop) {
            return switchToDesktop();
        }
        return run("systemctl", {verb(action)}, verb(action));
    }

  private:
    std::string switchToDesktop() {
        if (const std::string failure = exit_.request.write(); !failure.empty()) {
            return failure;
        }
        if (onPath(exit_.path, sessionSelect)) {
            lucent::info("power", "switching to the desktop with {}", sessionSelect);
            return run(sessionSelect, {"plasma"}, "switch to the desktop");
        }
        lucent::info("power", "switching to the desktop by stopping {}", exit_.compositorScope);
        return run("systemctl", {"--user", "stop", exit_.compositorScope}, "end the session");
    }

    /// Runs `program`; empty when it succeeded, else why it did not (`what` names the action).
    std::string run(const std::string& program, const std::vector<std::string>& args,
                    const std::string& what) {
        const std::optional<launch::Captured> out = run_(program, args);
        if (!out) {
            return program + " could not be run";
        }
        if (out->status == 0) {
            return {};
        }
        const std::string reason = firstLine(out->output);
        return reason.empty() ? program + " refused to " + what : reason;
    }

    Runner run_;
    SessionExit exit_;
};

class RefusingPower final : public Power {
  public:
    explicit RefusingPower(std::string reason) : reason_{std::move(reason)} {
    }

    std::string perform(PowerAction) override {
        return reason_;
    }

  private:
    std::string reason_;
};

} // namespace

const char* label(PowerAction action) noexcept {
    switch (action) {
    case PowerAction::Suspend:
        return "Sleep";
    case PowerAction::Restart:
        return "Restart";
    case PowerAction::ShutDown:
        return "Shut down";
    case PowerAction::SwitchToDesktop:
        return "Switch to desktop";
    }
    return "";
}

std::unique_ptr<Power> makeLogindPower(Runner run, SessionExit exit) {
    return std::make_unique<LogindPower>(std::move(run), std::move(exit));
}

std::unique_ptr<Power> makeRefusingPower(std::string reason) {
    return std::make_unique<RefusingPower>(std::move(reason));
}

} // namespace opensu::host
