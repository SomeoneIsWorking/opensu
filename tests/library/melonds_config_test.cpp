// melonDS.toml as a launch on the virtual pads reads it: joystick 0, the player's buttons kept,
// the DS layout on the virtual pad's codes when the player has none.
#include "library/melonds_config.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "library/ini_file.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::melondsLaunchSettings;
using opensu::test::expect;

// melonDS's own first file: every joystick button unbound.
constexpr std::string_view fresh = "[Instance0]\n"
                                   "JoystickID = 0\n"
                                   "\n"
                                   "[Instance0.Joystick]\n"
                                   "A = -1\n"
                                   "Up = -1\n"
                                   "\n"
                                   "[Instance0.Window0]\n"
                                   "Width = 256\n";

std::string value(std::string_view toml, const IniKey& key) {
    return iniValueIn(toml, key).value_or("<none>");
}

void testDefaults() {
    const std::string run = melondsLaunchSettings(fresh);
    expect(value(run, {"Instance0.Joystick", "A"}) == "1", "DS A is the virtual pad's east, b1");
    expect(value(run, {"Instance0.Joystick", "B"}) == "0", "DS B is its south, b0");
    expect(value(run, {"Instance0.Joystick", "Start"}) == "7", "Start is b7");
    expect(value(run, {"Instance0.Joystick", "Up"}) == std::to_string(0x01110101),
           "Up is hat 0 up or the left stick's -Y");
    expect(value(run, {"Instance0.Joystick", "Right"}) == std::to_string(0x00010102),
           "Right is hat 0 right or the left stick's +X");
    expect(value(run, {"Instance0", "JoystickID"}) == "0", "the first virtual pad is joystick 0");
    expect(value(run, {"Instance0.Window0", "Width"}) == "256", "other settings stay");
    expect(melondsLaunchSettings(run) == run, "a file already set stays as it is");
    expect(value(melondsLaunchSettings(""), {"Instance0.Joystick", "Y"}) == "2",
           "a player with no settings file plays the first virtual pad");
}

void testPlayerButtons() {
    const std::string player = "[Instance0]\nJoystickID = 3\n\n[Instance0.Joystick]\nA = 5\n";
    const std::string run = melondsLaunchSettings(player);
    expect(value(run, {"Instance0.Joystick", "A"}) == "5", "the player's buttons are kept");
    expect(value(run, {"Instance0.Joystick", "B"}) == "<none>", "and nothing is added to them");
    expect(value(run, {"Instance0", "JoystickID"}) == "0", "they play the first virtual pad");
}

} // namespace

int main() try {
    testDefaults();
    testPlayerButtons();
    std::printf("melonds_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
