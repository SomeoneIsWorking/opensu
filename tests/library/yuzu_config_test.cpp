// yuzu's line's qt-config.ini as a launch reads it: the update check off, SDL bindings on the
// virtual pads, a Pro Controller for a player without any; INI values set in place.
#include "library/yuzu_config.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "library/ini_file.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::library::iniValueIn;
using opensu::library::withIniValue;
using opensu::library::yuzuLaunchSettings;
using opensu::test::expect;

constexpr std::string_view guid = "0300cece5e0400008e02000010010000";

// Eden's own layout: player 1 on the keyboard, player 2 on an Xbox pad.
constexpr std::string_view player =
    "[Controls]\n"
    "player_0_connected\\default=true\n"
    "player_0_connected=true\n"
    "player_1_connected\\default=false\n"
    "player_1_connected=true\n"
    "player_0_button_a\\default=false\n"
    "player_0_button_a=\"engine:keyboard,code:67,toggle:0\"\n"
    "player_1_button_a\\default=false\n"
    "player_1_button_a=\"engine:sdl,guid:060000005e040000ea02000008040000,port:0,button:1\"\n"
    "player_1_lstick\\default=false\n"
    "player_1_lstick=\"engine:sdl,guid:060000005e040000ea02000008040000,axis_x:0,port:0,"
    "axis_y:1\"\n"
    "player_1_button_screenshot=[empty]\n"
    "\n"
    "[UI]\n"
    "check_for_updates\\default=true\n"
    "check_for_updates=true\n";

std::string value(std::string_view ini, std::string_view section, std::string_view key) {
    return iniValueIn(ini, section, key).value_or("<none>");
}

void testIniValue() {
    const std::string_view ini = "[A]\nk=1\n\n[B]\nk = 2\n";
    expect(value(ini, "B", "k") == "2", "a value is read from its section");
    expect(withIniValue(ini, "A", "k", "9") == "[A]\nk=9\n\n[B]\nk = 2\n", "a key is replaced");
    expect(withIniValue(ini, "A", "j", "3") == "[A]\nk=1\nj=3\n\n[B]\nk = 2\n",
           "a new key goes after the section's last setting");
    expect(withIniValue(ini, "C", "j", "3") == std::string{ini} + "[C]\nj=3\n",
           "a new section goes at the end");
    expect(withIniValue("", "C", "j", "3") == "[C]\nj=3\n", "an empty file gains the section");
}

void testUpdateCheck() {
    const std::string run = yuzuLaunchSettings(player, "");
    expect(value(run, "UI", "check_for_updates") == "false", "the update check is off");
    expect(value(run, "UI", "check_for_updates\\default") == "false",
           "Eden reads the value, not its default");
    expect(value(run, "Controls", "player_1_button_a") ==
               value(player, "Controls", "player_1_button_a"),
           "without virtual pads the bindings stay");
}

void testPads() {
    const std::string run = yuzuLaunchSettings(player, guid);
    expect(value(run, "Controls", "player_1_button_a") ==
               "\"engine:sdl,guid:" + std::string{guid} + ",port:1,button:1\"",
           "an SDL binding plays the Nth virtual pad, its button kept");
    expect(value(run, "Controls", "player_1_lstick") ==
               "\"engine:sdl,guid:" + std::string{guid} + ",axis_x:0,port:1,axis_y:1\"",
           "a stick is retargeted the same way");
    expect(value(run, "Controls", "player_0_button_a") ==
               "\"engine:sdl,guid:" + std::string{guid} + ",port:0,button:1\"",
           "a keyboard player 1 gets the first virtual pad's A");
    expect(value(run, "Controls", "player_0_button_zr") ==
               "\"engine:sdl,guid:" + std::string{guid} +
                   ",port:0,threshold:0.500000,axis:5,"
                   "invert:+\"",
           "ZR is the right trigger's axis");
    expect(value(run, "Controls", "player_0_button_screenshot") == "[empty]",
           "a button the virtual pad lacks is left unbound");
    expect(value(run, "Controls", "player_0_type") == "0", "player 1 is a Pro Controller");
    expect(value(run, "Controls", "player_0_connected") == "true", "player 1 is connected");
    expect(value(run, "Controls", "player_2_button_a") == "<none>",
           "a disconnected player without SDL bindings is left alone");
    expect(yuzuLaunchSettings(run, guid) == run, "a file already set stays as it is");
    const std::string fresh = yuzuLaunchSettings("", guid);
    expect(value(fresh, "Controls", "player_0_connected") == "true" &&
               value(fresh, "Controls", "player_0_button_b").find(",button:0") != std::string::npos,
           "a player with no settings file still plays the first virtual pad");
}

} // namespace

int main() try {
    testIniValue();
    testUpdateCheck();
    testPads();
    std::printf("yuzu_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
