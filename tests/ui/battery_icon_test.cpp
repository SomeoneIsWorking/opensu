// BatteryIcon's level-to-fill rule and iiSU a32.p's charging thresholds.
#include "battery_icon.hpp"

#include <cmath>
#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::BatteryIcon;

bool near(float a, float b) {
    return std::abs(a - b) < 0.001f;
}

void fillIsTheExactPercentage() {
    expect(near(BatteryIcon::fillWidth(37, 200.0f, 1.0f), 74.0f), "37% of 200");
    expect(near(BatteryIcon::fillWidth(65, 200.0f, 1.0f), 130.0f), "65% of 200");
    expect(near(BatteryIcon::fillWidth(66, 200.0f, 1.0f), 132.0f), "one percent is one step");
    expect(near(BatteryIcon::fillWidth(100, 200.0f, 1.0f), 200.0f), "100% is the whole width");
}

void endsOfTheRange() {
    expect(BatteryIcon::fillWidth(0, 200.0f, 1.0f) == 0.0f, "0% shows nothing");
    expect(near(BatteryIcon::fillWidth(1, 20.0f, 1.0f), 1.0f), "1% is at least the sliver");
    expect(near(BatteryIcon::fillWidth(1, 0.5f, 1.0f), 0.5f), "the sliver stays inside");
    expect(BatteryIcon::fillWidth(-5, 200.0f, 1.0f) == 0.0f, "below 0 is empty");
    expect(near(BatteryIcon::fillWidth(140, 200.0f, 1.0f), 200.0f), "above 100 is full");
}

void boltThresholds() {
    expect(BatteryIcon::boltColour(100) == 0x00FF48, "charged");
    expect(BatteryIcon::boltColour(80) == 0x00FF48, "80 is green");
    expect(BatteryIcon::boltColour(79) == 0xFFC000 && BatteryIcon::boltColour(60) == 0xFFC000,
           "60 to 79 is yellow");
    expect(BatteryIcon::boltColour(59) == 0xFF7200 && BatteryIcon::boltColour(30) == 0xFF7200,
           "30 to 59 is orange");
    expect(BatteryIcon::boltColour(29) == 0xFF0000 && BatteryIcon::boltColour(0) == 0xFF0000,
           "below 30 is red");
}

} // namespace

int main() try {
    fillIsTheExactPercentage();
    endsOfTheRange();
    boltThresholds();
    std::printf("battery_icon: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
