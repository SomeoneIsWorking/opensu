// controller_roster — the pads connected now as the Devices page and the quick menu list them: in
// player order (the order they connected in), with the battery each reports and the buttons held
// on each right now, which is the live button test. Reads the pad layer through a function, so
// tests hand it fake pads.
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "device/battery.hpp"
#include "gamepad/event.hpp"
#include "gamepad/pads.hpp"

namespace opensu::app {

/// One connected pad.
struct ControllerStatus {
    /// The pad layer's id for it, which button events name as their source.
    std::string id;
    std::string name;
    /// 1 for the first pad connected.
    std::size_t player{1};
    std::optional<device::BatteryStatus> battery;
    /// The buttons held now, in the order they went down.
    std::vector<gamepad::Button> held;
};

class ControllerRoster {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long a battery reading is kept before the supply is read again.
    static constexpr std::chrono::seconds batteryLife{5};

    struct Sources {
        /// The pads connected now, in connection order.
        std::function<std::vector<gamepad::PadInfo>()> pads;
        /// The battery of the pad with this unique id.
        std::function<std::optional<device::BatteryStatus>(const std::string& uniq)> battery;
    };

    explicit ControllerRoster(Sources sources) : sources_{std::move(sources)} {
    }

    /// Notes a button going down or up on a pad. Returns whether a listed pad's held set changed.
    bool note(const gamepad::Event& event);
    /// The pads now, each with what it holds and its battery as of `now`.
    [[nodiscard]] std::vector<ControllerStatus> controllers(Clock::time_point now);

  private:
    struct Reading {
        std::optional<device::BatteryStatus> battery;
        Clock::time_point at{};
    };

    Sources sources_;
    std::map<std::string, std::vector<gamepad::Button>> held_;
    std::map<std::string, Reading> batteries_;
};

} // namespace opensu::app
