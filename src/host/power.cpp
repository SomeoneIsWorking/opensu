#include "power.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

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
    }
    return "";
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
    explicit LogindPower(Runner run) : run_{std::move(run)} {
    }

    std::string perform(PowerAction action) override {
        const std::optional<launch::Captured> out = run_("systemctl", {verb(action)});
        if (!out) {
            return "systemctl could not be run";
        }
        if (out->status == 0) {
            return {};
        }
        const std::string reason = firstLine(out->output);
        return reason.empty() ? std::string{"systemctl refused to "} + verb(action) : reason;
    }

  private:
    Runner run_;
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
    }
    return "";
}

std::unique_ptr<Power> makeLogindPower(Runner run) {
    return std::make_unique<LogindPower>(std::move(run));
}

std::unique_ptr<Power> makeRefusingPower(std::string reason) {
    return std::make_unique<RefusingPower>(std::move(reason));
}

} // namespace opensu::host
