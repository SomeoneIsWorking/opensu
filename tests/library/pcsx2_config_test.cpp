// PCSX2.ini as a launch reads it: the setup wizard off, SDL bindings on the Nth virtual pad, a
// DualShock 2 bound only to the keyboard given PCSX2's SDL layout.
#include "library/pcsx2_config.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "library/ini_file.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::library::IniKey;
using opensu::library::iniKeysIn;
using opensu::library::iniValueIn;
using opensu::library::pcsx2LaunchSettings;
using opensu::library::pcsx2PadSettings;
using opensu::test::expect;

// The player's own layout: pad 1 on the keyboard, pad 2 on their second SDL pad, pad 3 a guitar.
constexpr std::string_view player = "[UI]\n"
                                    "SettingsVersion = 1\n"
                                    "SetupWizardIncomplete = true\n"
                                    "\n"
                                    "[Pad1]\n"
                                    "Type = DualShock2\n"
                                    "Cross = Keyboard/K\n"
                                    "Start = Keyboard/Return\n"
                                    "\n"
                                    "[Pad2]\n"
                                    "Type = DualShock2\n"
                                    "Cross = SDL-3/FaceSouth\n"
                                    "L2 = SDL-3/+LeftTrigger & Keyboard/Shift\n"
                                    "\n"
                                    "[Pad3]\n"
                                    "Type = Guitar\n"
                                    "Green = Keyboard/1\n";

std::string value(std::string_view ini, const IniKey& key) {
    return iniValueIn(ini, key).value_or("<none>");
}

void testWizard() {
    const std::string run = pcsx2LaunchSettings(player, false);
    expect(value(run, {"UI", "SetupWizardIncomplete"}) == "false", "the setup wizard is off");
    expect(value(run, {"Pad1", "Cross"}) == "Keyboard/K", "without virtual pads the bindings stay");
    const std::string fresh = pcsx2LaunchSettings("", false);
    expect(value(fresh, {"UI", "SettingsVersion"}) == "1",
           "a made file has the version PCSX2 keeps it at");
    const std::string set = "[UI]\nSettingsVersion = 1\nSetupWizardIncomplete = false\n";
    expect(pcsx2LaunchSettings(set, false) == set, "a file already set stays byte for byte");
}

void testPads() {
    const std::string run = pcsx2LaunchSettings(player, true);
    expect(value(run, {"Pad1", "Cross"}) == "SDL-0/FaceSouth",
           "a keyboard DualShock 2 gets the first virtual pad's Cross");
    expect(value(run, {"Pad1", "L2"}) == "SDL-0/+LeftTrigger", "L2 is the left trigger");
    expect(value(run, {"Pad1", "LUp"}) == "SDL-0/-LeftY", "the left stick's up is its -Y");
    expect(value(run, {"Pad1", "Analog"}) == "<none>", "Analog is not put on Guide");
    expect(value(run, {"Pad2", "Cross"}) == "SDL-1/FaceSouth",
           "an SDL binding plays the Nth virtual pad, its input kept");
    expect(value(run, {"Pad2", "L2"}) == "SDL-1/+LeftTrigger & Keyboard/Shift",
           "a chord's SDL part is retargeted, the rest kept");
    expect(value(run, {"Pad2", "Start"}) == "<none>", "a pad bound by SDL gets no defaults");
    expect(value(run, {"Pad3", "Green"}) == "Keyboard/1", "a guitar on the keyboard stays");
    expect(value(run, {"Pad4", "Cross"}) == "<none>", "a pad not connected stays unbound");
    expect(pcsx2PadSettings(run) == run, "a file already bound stays as it is");
    const std::string fresh = pcsx2LaunchSettings("", true);
    expect(value(fresh, {"Pad1", "Type"}) == "DualShock2" &&
               value(fresh, {"Pad1", "Start"}) == "SDL-0/Start",
           "a player with no settings file plays the first virtual pad");
    expect(iniKeysIn(player, "Pad2") == std::vector<std::string>{"Type", "Cross", "L2"},
           "a section's keys are listed in order");
}

} // namespace

int main() try {
    testWizard();
    testPads();
    std::printf("pcsx2_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
