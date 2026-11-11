// Azahar's qt-config.ini as a launch reads it: the update check on start off, the active
// profile's SDL bindings on the first virtual pad, the standard layout for a keyboard profile.
#include "library/azahar_config.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include "library/ini_file.hpp"
#include "support/launch_writes.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::library::azaharId;
using opensu::library::azaharLaunchSettings;
using opensu::library::azaharSettingsFile;
using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::LaunchSetup;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;
using opensu::test::writtenOver;

// Azahar keys a pad by SDL's full GUID, the name's CRC kept.
constexpr std::string_view guid = "0300cece5e0400008e02000010010000";

// The player's layout: profile 1 on the keyboard, profile 2, the active one, on their own pad.
constexpr std::string_view player =
    "[Controls]\n"
    "profile=1\n"
    "profile\\default=false\n"
    "profiles\\1\\button_a=\"code:65,engine:keyboard\"\n"
    "profiles\\1\\button_a\\default=true\n"
    "profiles\\2\\button_a=\"api:controller,button:1,engine:sdl,guid:"
    "060000005e040000ea02000008040000,port:0\"\n"
    "profiles\\2\\button_a\\default=false\n"
    "profiles\\2\\button_b=\"code:83,engine:keyboard\"\n"
    "profiles\\size=2\n"
    "\n"
    "[Miscellaneous]\n"
    "check_for_update_on_start=true\n"
    "check_for_update_on_start\\default=true\n";

std::string value(std::string_view ini, const IniKey& key) {
    return iniValueIn(ini, key).value_or("<none>");
}

void testUpdateCheck() {
    const std::string run = azaharLaunchSettings(player, "");
    expect(value(run, {"Miscellaneous", "check_for_update_on_start"}) == "false",
           "the update check on start is off");
    expect(value(run, {"Miscellaneous", "check_for_update_on_start\\default"}) == "false",
           "Azahar reads the value, not its default");
    expect(value(run, {"Controls", "profiles\\2\\button_a"}) ==
               value(player, {"Controls", "profiles\\2\\button_a"}),
           "without virtual pads the bindings stay");
}

void testPads() {
    const std::string run = azaharLaunchSettings(player, guid);
    expect(value(run, {"Controls", "profiles\\2\\button_a"}) ==
               "\"api:controller,button:1,engine:sdl,guid:" + std::string{guid} + ",port:0\"",
           "the active profile's SDL binding plays the first virtual pad, its button kept");
    expect(value(run, {"Controls", "profiles\\2\\button_b"}) == "\"code:83,engine:keyboard\"",
           "a key in a profile with SDL bindings stays");
    expect(value(run, {"Controls", "profiles\\1\\button_a"}) == "\"code:65,engine:keyboard\"",
           "a profile Azahar does not use stays");
    expect(azaharLaunchSettings(run, guid) == run, "a file already set stays as it is");

    const std::string keyboard = azaharLaunchSettings(
        "[Controls]\nprofile=0\nprofiles\\1\\button_a=\"code:65,engine:keyboard\"\n"
        "profiles\\size=1\n",
        guid);
    expect(value(keyboard, {"Controls", "profiles\\1\\button_a"}) ==
               "\"api:controller,engine:sdl,guid:" + std::string{guid} + ",port:0,button:1\"",
           "a keyboard profile gets 3DS A on the virtual pad's east button");
    expect(value(keyboard, {"Controls", "profiles\\1\\button_a\\default"}) == "false",
           "Azahar reads the binding, not its default");
    expect(value(keyboard, {"Controls", "profiles\\1\\circle_pad"}).find("axis_x:0,axis_y:1") !=
               std::string::npos,
           "the circle pad is the left stick");

    const std::string fresh = azaharLaunchSettings("", guid);
    expect(value(fresh, {"Controls", "profiles\\size"}) == "1" &&
               value(fresh, {"Controls", "profiles\\1\\button_b"}).find(",button:0") !=
                   std::string::npos,
           "a player with no settings file still plays the first virtual pad");
}

void testWrites() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "azahar-config"};
    const LaunchSetup setup{.emulatorId = std::string{azaharId},
                            .config = scratch.path(),
                            .data = {},
                            .padGuid = std::string{guid}};
    expect(writtenOver(setup, scratch.path() / azaharSettingsFile, player) ==
               azaharLaunchSettings(player, guid),
           "Azahar's settings are written in place");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testUpdateCheck();
    testPads();
    testWrites();
    std::printf("azahar_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
