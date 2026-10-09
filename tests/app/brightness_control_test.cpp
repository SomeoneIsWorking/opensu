// Brightness through the backlight: absent without a device, stepped and clamped with one.
#include <cstdio>

#include "brightness_control.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void withoutABacklightThereIsNoBrightness() {
    app::BrightnessControl control{nullptr};
    expect(!control.available() && !control.percent(), "nothing to read");
    expect(!control.set(30).empty() && !control.step(5).empty(), "changes are refused");
}

void stepsAndClamps() {
    test::FakeBacklight backlight;
    app::BrightnessControl control{&backlight};
    expect(control.available() && control.percent() == 50, "reads the level");
    expect(control.step(app::brightnessStep).empty() && backlight.level == 55, "steps up");
    expect(control.step(-100).empty() && backlight.level >= 1, "never goes fully dark");
    expect(control.step(500).empty() && backlight.level == 100, "tops out at 100");
}

} // namespace

int main() {
    withoutABacklightThereIsNoBrightness();
    stepsAndClamps();
    std::printf("brightness_control: all checks passed\n");
    return 0;
}
