// ClockText against iiSU's o28.g formats and k42's minute ticker.
#include "clock_text.hpp"

#include <cstdio>

#include "battery_icon.hpp"
#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::BatteryIcon;
using opensu::ui::ClockText;

void twentyFourHour() {
    expect(ClockText::format(0, 5, true) == "00:05", "HH:mm pads the hour");
    expect(ClockText::format(13, 7, true) == "13:07", "HH:mm afternoon");
    expect(!ClockText::hasLetters(ClockText::format(23, 59, true)), "24 h has no letters");
}

void twelveHour() {
    expect(ClockText::format(0, 5, false) == "12:05 AM", "midnight is 12 AM");
    expect(ClockText::format(9, 30, false) == "9:30 AM", "h:mm has no leading zero");
    expect(ClockText::format(12, 0, false) == "12:00 PM", "noon is 12 PM");
    expect(ClockText::format(13, 7, false) == "1:07 PM", "afternoon");
    expect(ClockText::hasLetters(ClockText::format(13, 7, false)), "12 h has letters");
}

void minuteBoundary() {
    expect(ClockText::untilNextMinute(0, 0).count() == 60000, "on the boundary, a whole minute");
    expect(ClockText::untilNextMinute(30, 250).count() == 29750, "to the next boundary");
    expect(ClockText::untilNextMinute(59, 500).count() == 1000, "at least one second");
    expect(ClockText::untilNextMinute(60, 0).count() == 1000, "a leap second still waits one");
}

void batterySlots() {
    // iiSU a32.p thresholds.
    expect(BatteryIcon::slot(80, false) == 0 && BatteryIcon::slot(79, false) == 1, "80 is full");
    expect(BatteryIcon::slot(60, false) == 1 && BatteryIcon::slot(40, false) == 2, "60 and 40");
    expect(BatteryIcon::slot(20, false) == 3 && BatteryIcon::slot(19, false) == 4, "below 20");
    expect(BatteryIcon::slot(100, true) == 0 && BatteryIcon::slot(99, true) == 1, "charged");
    expect(BatteryIcon::slot(60, true) == 2 && BatteryIcon::slot(59, true) == 3, "charging 60");
    expect(BatteryIcon::slot(30, true) == 3 && BatteryIcon::slot(29, true) == 4, "charging 30");
}

} // namespace

int main() {
    twentyFourHour();
    twelveHour();
    minuteBoundary();
    batterySlots();
    std::printf("clock_text: all checks passed\n");
    return 0;
}
