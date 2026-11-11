// RPCS3's input profile as a launch reads it: player N on `Xbox 360 Controller N`, an SDL
// player's mappings kept, player 1 or a keyboard player given RPCS3's SDL layout; the welcome
// dialog off.
#include "library/rpcs3_config.hpp"

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

using opensu::library::iniValueIn;
using opensu::library::LaunchSetup;
using opensu::library::rpcs3DefaultProfile;
using opensu::library::rpcs3GuiLaunchSettings;
using opensu::library::rpcs3Id;
using opensu::library::rpcs3PadProfile;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;
using opensu::test::writtenOver;

// The player's own: 1 and 3 on the keyboard, 2 off, 4 on an SDL pad.
constexpr std::string_view player = "Player 1 Input:\n"
                                    "  Handler: Keyboard\n"
                                    "  Device: Keyboard\n"
                                    "  Config:\n"
                                    "    Start: Return\n"
                                    "    Cross: X\n"
                                    "    Motion Sensor X:\n"
                                    "      Axis: \"\"\n"
                                    "    Left Stick Deadzone: 0\n"
                                    "  Buddy Device: \"\"\n"
                                    "Player 2 Input:\n"
                                    "  Handler: \"Null\"\n"
                                    "  Device: Xbox One Wireless Controller 1\n"
                                    "  Config:\n"
                                    "    Cross: \"\"\n"
                                    "  Buddy Device: \"Null\"\n"
                                    "Player 3 Input:\n"
                                    "  Handler: Keyboard\n"
                                    "  Device: Keyboard\n"
                                    "  Config:\n"
                                    "    Cross: X\n"
                                    "Player 4 Input:\n"
                                    "  Handler: SDL\n"
                                    "  Device: DualSense Wireless Controller 1\n"
                                    "  Config:\n"
                                    "    Cross: East\n";

bool has(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
}

void testPads() {
    const std::string run = rpcs3PadProfile(player);
    expect(has(run, "Player 1 Input:\n  Handler: SDL\n  Device: Xbox 360 Controller 1\n"),
           "a keyboard player 1 plays the first virtual pad by SDL");
    expect(has(run, "    Start: Start\n") && has(run, "    Cross: South\n"),
           "and gets RPCS3's SDL layout");
    expect(has(run, "    Left Stick Left: LS X-\n"), "the stick is bound too");
    expect(has(run, "      Axis: \"\"\n    Left Stick Deadzone: 0\n"),
           "settings that are not buttons stay");
    expect(has(run,
               "Player 2 Input:\n  Handler: \"Null\"\n  Device: Xbox One Wireless Controller 1\n"),
           "a player off stays off");
    expect(has(run, "Player 3 Input:\n  Handler: SDL\n  Device: Xbox 360 Controller 3\n"),
           "a keyboard player N plays the Nth virtual pad");
    expect(has(run, "Player 4 Input:\n  Handler: SDL\n  Device: Xbox 360 Controller 4\n"
                    "  Config:\n    Cross: East\n"),
           "an SDL player is retargeted, its mappings kept");
    expect(rpcs3PadProfile(run) == run, "a profile already bound stays as it is");
    const std::string fresh = rpcs3PadProfile("");
    expect(fresh.starts_with("Player 1 Input:\n  Handler: SDL\n  Device: Xbox 360 Controller 1\n"
                             "  Config:\n") &&
               has(fresh, "    Cross: South\n"),
           "a player without a profile plays the first virtual pad");
}

void testWelcome() {
    const std::string run = rpcs3GuiLaunchSettings("[main_window]\ninfoBoxEnabledWelcome=true\n");
    expect(iniValueIn(run, {"main_window", "infoBoxEnabledWelcome"}) == "false",
           "the welcome dialog is off");
    const std::string set = "[main_window]\ninfoBoxEnabledWelcome=false\n";
    expect(rpcs3GuiLaunchSettings(set) == set, "a file already set stays byte for byte");
}

void testWrites() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "rpcs3-config"};
    const LaunchSetup setup{.emulatorId = std::string{rpcs3Id},
                            .config = scratch.path(),
                            .data = {},
                            .padGuid = std::string{"0300cece5e0400008e02000010010000"}};
    expect(writtenOver(setup, scratch.path() / rpcs3DefaultProfile, player) ==
               rpcs3PadProfile(player),
           "RPCS3's default input profile are written in place");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testPads();
    testWelcome();
    testWrites();
    std::printf("rpcs3_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
