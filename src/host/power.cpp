#include "power.hpp"

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

class LogindPower final : public Power {
  public:
    explicit LogindPower(Runner run) : run_{std::move(run)} {
    }

    std::string perform(PowerAction action) override {
        return refusalOf("systemctl", run_("systemctl", {verb(action)}), verb(action));
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
