// ClockText against iiSU's o28.g formats and k42's minute ticker.
#include "clock_text.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::test::expect;
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

void dateLine() {
    expect(ClockText::date(3, 11, 10) == "Wed 11 Nov", "weekday, day, month");
    expect(ClockText::date(0, 1, 0) == "Sun 1 Jan", "the first day and month");
    expect(ClockText::date(7, 1, 0).empty() && ClockText::date(1, 1, 12).empty(),
           "out of range reads empty");
}

} // namespace

int main() try {
    twentyFourHour();
    twelveHour();
    minuteBoundary();
    dateLine();
    std::printf("clock_text: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
