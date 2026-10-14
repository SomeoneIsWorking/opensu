// top_bar_clock — the top bar's clock, set again on each minute boundary as iiSU's k42 re-renders
// it, with the battery read on the same tick.
#pragma once

#include <chrono>

#include "device/battery.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class TopBarClock {
  public:
    using Clock = std::chrono::steady_clock;

    /// Writes into `shell`; `hour24` is the clock's format.
    TopBarClock(ui::Shell& shell, bool hour24) noexcept : shell_{shell}, hour24_{hour24} {
    }

    /// Sets the clock and the battery on the first call and once each minute has turned.
    void service(Clock::time_point now);

  private:
    ui::Shell& shell_;
    bool hour24_;
    device::BatteryReader battery_;
    /// When the clock next changes, on the minute boundary.
    Clock::time_point next_{};
};

} // namespace opensu::app
