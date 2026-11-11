// DuckStation's settings.ini as a launch writes it: the update check, setup wizard and launcher
// question off, SDL bindings on the Nth virtual pad, a pad bound only to the keyboard given
// DuckStation's SDL layout; a second launch writes nothing.
#include "library/duckstation_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "library/emulator_launch_config.hpp"
#include "library/ini_file.hpp"
#include "support/launch_writes.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;

using opensu::library::duckstationLaunchSettings;
using opensu::library::duckstationPadSettings;
using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::LaunchSetup;
using opensu::library::writeLaunchSettings;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

// Pad 1 on the keyboard, pad 2 on a DualSense by its persistent identifier, pad 3 a mouse.
constexpr std::string_view player = "[Main]\n"
                                    "SettingsVersion = 3\n"
                                    "\n"
                                    "[Pad1]\n"
                                    "Type = AnalogController\n"
                                    "Cross = Keyboard/K\n"
                                    "\n"
                                    "[Pad2]\n"
                                    "Type = AnalogController\n"
                                    "Cross = SDL-0300c0ffee-a0b1c2d3e4f5/A\n"
                                    "L2 = SDL-1/+LeftTrigger & Keyboard/Shift\n"
                                    "\n"
                                    "[Pad3]\n"
                                    "Type = PlayStationMouse\n"
                                    "Left = Pointer-0/LeftButton\n";

std::string value(std::string_view ini, const IniKey& key) {
    return iniValueIn(ini, key).value_or("<none>");
}

void testPrompts() {
    const std::string run = duckstationLaunchSettings(player, false);
    expect(value(run, {"AutoUpdater", "CheckAtStartup"}) == "false", "the update check is off");
    expect(value(run, {"Main", "SetupWizardIncomplete"}) == "false", "the setup wizard is off");
    expect(value(run, {"Main", "NoDesktopFile"}) == "true", "the launcher question is off");
    expect(value(run, {"Pad1", "Cross"}) == "Keyboard/K", "without virtual pads the bindings stay");
    expect(duckstationLaunchSettings(run, false) == run, "a file already set stays byte for byte");
}

void testPads() {
    const std::string run = duckstationLaunchSettings(player, true);
    expect(value(run, {"Pad1", "Cross"}) == "SDL-0/A",
           "a keyboard controller gets the first virtual pad's south button for Cross");
    expect(value(run, {"Pad1", "L2"}) == "SDL-0/+LeftTrigger", "L2 is the left trigger");
    expect(value(run, {"Pad1", "LUp"}) == "SDL-0/-LeftY", "the left stick's up is its -Y");
    expect(value(run, {"Pad1", "Analog"}) == "<none>", "Analog is not put on Guide");
    expect(value(run, {"Pad2", "Cross"}) == "SDL-1/A",
           "a persistent identifier is retargeted to the Nth virtual pad, its input kept");
    expect(value(run, {"Pad2", "L2"}) == "SDL-1/+LeftTrigger & Keyboard/Shift",
           "a chord's SDL part is retargeted, the rest kept");
    expect(value(run, {"Pad2", "Start"}) == "<none>", "a pad bound by SDL gets no defaults");
    expect(value(run, {"Pad3", "Left"}) == "Pointer-0/LeftButton", "a mouse stays as it is");
    expect(value(run, {"Pad4", "Cross"}) == "<none>", "a pad not connected stays unbound");
    expect(duckstationPadSettings(run) == run, "a file already bound stays as it is");
    const std::string fresh = duckstationLaunchSettings("", true);
    expect(value(fresh, {"Pad1", "Type"}) == "AnalogController" &&
               value(fresh, {"Pad1", "Start"}) == "SDL-0/Start",
           "a player with no settings file plays the first virtual pad");
    const std::string digital =
        duckstationPadSettings("[Pad1]\nType = DigitalController\nCross = Keyboard/K\n");
    expect(value(digital, {"Pad1", "Type"}) == "DigitalController" &&
               value(digital, {"Pad1", "Cross"}) == "SDL-0/A",
           "a digital pad keeps its type and gets the layout");
}

void testWrites() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "duckstation-config"};
    const fs::path& root = scratch.path();
    fs::create_directories(root / "inputprofiles");
    std::ofstream{root / "inputprofiles" / "Pad.ini", std::ios::binary}
        << "[Pad1]\nType = AnalogController\nCross = SDL-3/A\n";
    const LaunchSetup setup{.emulatorId = "duckstation",
                            .config = root,
                            .data = {},
                            .padGuid = "0300cece5e0400008e02000010010000"};
    writeLaunchSettings(setup);
    const std::string made = read(root / "settings.ini");
    expect(made == duckstationLaunchSettings("", true),
           "the settings file the player lacks is made");
    expect(value(made, {"Main", "NoDesktopFile"}) == "true" &&
               value(made, {"Pad1", "Cross"}) == "SDL-0/A",
           "the made settings file has the prompts off and the first virtual pad");
    expect(value(read(root / "inputprofiles" / "Pad.ini"), {"Pad1", "Cross"}) == "SDL-0/A",
           "an input profile's pad 1 plays the first virtual pad");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testPrompts();
    testPads();
    testWrites();
    std::printf("duckstation_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
