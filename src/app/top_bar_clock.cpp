#include "top_bar_clock.hpp"

#include <ctime>

#include "clock/local_time.hpp"
#include "ui/clock_text.hpp"

namespace opensu::app {

void TopBarClock::service(Clock::time_point now) {
    if (now < next_) {
        return;
    }
    const auto wall = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(wall);
    const std::tm parts = clock::localTime(seconds);
    const auto millisecond = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(wall.time_since_epoch()).count() %
        1000);
    shell_.setClock(ui::ClockText::format(parts.tm_hour, parts.tm_min, hour24_));
    next_ = now + ui::ClockText::untilNextMinute(parts.tm_sec, millisecond);
    // STOPGAP: the battery is re-read on the clock's minute tick because opensu has no
    // power_supply uevent listener standing in for iiSU's ACTION_BATTERY_CHANGED receiver.
    shell_.setBattery(battery_.read());
}

} // namespace opensu::app
