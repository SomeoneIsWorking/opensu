// Dolphin's pad settings and per-game profiles written bound to the virtual pads, a keyboard
// GameCube slot given a standard pad, each Wii game's remote emulated as the game wants it,
// analytics' prompt marked asked; a second launch writes nothing.
#include "library/dolphin_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "library/emulator_launch_config.hpp"
#include "library/ini_file.hpp"
#include "library/wii_controls.hpp"
#include "support/launch_writes.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::DolphinController;
using opensu::library::dolphinSlots;
using opensu::library::dolphinSlotSettings;
using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::LaunchSetup;
using opensu::library::WiiControls;
using opensu::library::WiiRemote;
using opensu::library::writeLaunchSettings;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;

constexpr const char* guid = "0300cece5e0400008e02000010010000";

// Dolphin 2503a's own layout: GameCube pad 1 on keyboard and mouse, pad 2 on an SDL pad.
constexpr std::string_view gcPads = "[GCPad1]\n"
                                    "Device = XInput2/0/Virtual core pointer\n"
                                    "Buttons/A = `X`\n"
                                    "Main Stick/Up = `Up`\n"
                                    "Main Stick/Calibration = 100.00 141.42 100.00 141.42\n"
                                    "[GCPad2]\n"
                                    "Device = SDL/0/Xbox One Controller\n"
                                    "Buttons/A = `Button E`\n";

constexpr std::string_view wiimotes = "[Wiimote1]\n"
                                      "Device = XInput2/0/Virtual core pointer\n"
                                      "Buttons/A = `Click 1`\n"
                                      "Source = 1\n";

constexpr std::string_view mainSettings = "[Core]\n"
                                          "SIDevice0 = 6\n";

constexpr std::string_view gameSettings = "[Controls]\n"
                                          "WiimoteProfile1 = sideways\n"
                                          "PadProfile2 = pads\n";

constexpr std::string_view sideways = "[Profile]\n"
                                      "Device = SDL/0/DualSense Wireless Controller\n"
                                      "Buttons/2 = `Button S`\n";

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void write(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

std::optional<std::string> value(std::string_view text, std::string_view section,
                                 std::string_view key) {
    return iniValueIn(text, IniKey{.section = section, .key = key});
}

void testSlots() {
    std::string run =
        dolphinSlotSettings(gcPads, "GCPad1", DolphinController::GameCube, 1, nullptr);
    run = dolphinSlotSettings(run, "GCPad2", DolphinController::GameCube, 2, nullptr);
    expect(value(run, "GCPad1", "Device") == "SDL/0/Xbox 360 Controller" &&
               value(run, "GCPad1", "Buttons/A") == "`Button S`" &&
               value(run, "GCPad1", "D-Pad/Up") == "`Pad N`",
           "a keyboard GameCube slot gets a standard pad on the first virtual pad");
    expect(value(run, "GCPad1", "Main Stick/Calibration") == "100.00 141.42 100.00 141.42",
           "its other settings are kept");
    expect(value(run, "GCPad2", "Device") == "SDL/1/Xbox 360 Controller" &&
               value(run, "GCPad2", "Buttons/A") == "`Button E`",
           "an SDL slot moves to the second virtual pad with its mappings");
    expect(dolphinSlotSettings(wiimotes, "Wiimote1", DolphinController::WiiRemote, 1, nullptr) ==
               wiimotes,
           "a keyboard Wii Remote is left as it is");
    expect(value(dolphinSlotSettings(sideways, "Profile", DolphinController::WiiRemote, 1, nullptr),
                 "Profile", "Device") == "SDL/0/Xbox 360 Controller",
           "an SDL Wii Remote profile moves to the virtual pad");
}

void testLaunch() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "dolphin-config"};
    const fs::path config = scratch.path() / "config";
    const fs::path data = scratch.path() / "data";
    const fs::path profile = config / "Profiles" / "Wiimote" / "sideways.ini";
    const fs::path folderProfile = config / "Profiles" / "GCPad" / "pads" / "one.ini";
    write(config / "GCPadNew.ini", gcPads);
    write(config / "WiimoteNew.ini", wiimotes);
    write(config / "Dolphin.ini", mainSettings);
    write(data / "GameSettings" / "SF8E01.ini", gameSettings);
    write(profile, sideways);
    write(folderProfile, sideways);
    const LaunchSetup setup{
        .emulatorId = "dolphin", .config = config, .data = data, .padGuid = guid};
    writeLaunchSettings(setup);
    std::string pads{gcPads};
    for (int slot = 1; slot <= dolphinSlots; ++slot) {
        pads = dolphinSlotSettings(pads, "GCPad" + std::to_string(slot),
                                   DolphinController::GameCube, slot, nullptr);
    }
    expect(read(config / "GCPadNew.ini") == pads,
           "the pad settings are written on the virtual pads");
    expect(read(config / "WiimoteNew.ini") == wiimotes, "a file the rule leaves alone is kept");
    expect(value(read(profile), "Profile", "Device") == "SDL/0/Xbox 360 Controller",
           "a per-game Wii Remote profile reads the player's virtual pad");
    expect(value(read(folderProfile), "Profile", "Device") == "SDL/1/Xbox 360 Controller",
           "a folder of per-game profiles reads the slot's virtual pad");
    const std::string settings = read(config / "Dolphin.ini");
    expect(value(settings, "Analytics", "PermissionAsked") == "True",
           "analytics' prompt is marked asked");
    expect(value(settings, "Interface", "SkipNKitWarning") == "True",
           "an NKit disc starts without its warning");
    expect(value(settings, "Core", "SIDevice0") == "6", "the player's other settings stay");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

void testOwnPads() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "dolphin-config-own"};
    const fs::path config = scratch.path() / "config";
    write(config / "GCPadNew.ini", gcPads);
    const LaunchSetup setup{.emulatorId = "dolphin", .config = config, .data = {}, .padGuid = {}};
    writeLaunchSettings(setup);
    expect(read(config / "GCPadNew.ini") == gcPads,
           "a game reading the pads themselves keeps the player's pads");
    expect(value(read(config / "Dolphin.ini"), "Analytics", "PermissionAsked") == "True",
           "and still gets no analytics prompt, in a settings file made for it");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

// The player's own: a real Wii Remote in slot 1, nothing in the others, passthrough on.
constexpr std::string_view realRemote = "[Wiimote1]\n"
                                        "Device = XInput2/0/Virtual core pointer\n"
                                        "Buttons/A = `Click 1`\n"
                                        "Extension = Nunchuk\n"
                                        "Source = 2\n"
                                        "[Wiimote2]\n"
                                        "Device = XInput2/0/Virtual core pointer\n"
                                        "Source = 0\n";

constexpr std::string_view passthrough = "[BluetoothPassthrough]\n"
                                         "Enabled = True\n";

WiiRemote remoteOf(std::string_view gameId) {
    std::string body;
    {
        std::ifstream in{fs::path{OPENSU_ASSETS_DIR} / "wii-controls.json", std::ios::binary};
        body.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    }
    std::string error;
    const std::optional<WiiControls> controls = WiiControls::parse(body, error);
    expect(controls.has_value(), "the shipped Wii controls are read");
    return controls.value_or(WiiControls{}).remoteFor(gameId);
}

void testWiiGame() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "dolphin-config-wii"};
    const fs::path config = scratch.path() / "config";
    const fs::path data = scratch.path() / "data";
    const fs::path profile = config / "Profiles" / "Wiimote" / "sideways.ini";
    write(config / "WiimoteNew.ini", realRemote);
    write(config / "Dolphin.ini", passthrough);
    write(data / "GameSettings" / "SF8E01.ini", gameSettings);
    write(profile, sideways);
    const LaunchSetup setup{.emulatorId = "dolphin",
                            .config = config,
                            .data = data,
                            .padGuid = guid,
                            .wiiRemote = remoteOf("SF8E01")};
    writeLaunchSettings(setup);
    const std::string run = read(config / "WiimoteNew.ini");
    expect(value(run, "Wiimote1", "Source") == "1" &&
               value(run, "Wiimote1", "Device") == "SDL/0/Xbox 360 Controller",
           "a real remote's slot is emulated on the first virtual pad");
    expect(value(run, "Wiimote1", "Extension") == "None" &&
               value(run, "Wiimote1", "Options/Sideways Wiimote") == "True" &&
               value(run, "Wiimote1", "Buttons/2") == "`Button S`" &&
               value(run, "Wiimote1", "Shake/X") == "`Button E` | `Shoulder R`",
           "Donkey Kong Country Returns gets a sideways remote, shake on a button");
    expect(value(run, "Wiimote1", "IR/Up") == "",
           "a game without the pointer has none on the right stick");
    expect(value(run, "Wiimote2", "Source") == "0" &&
               value(run, "Wiimote2", "Device") == "XInput2/0/Virtual core pointer",
           "a slot with no remote stays empty");
    expect(value(read(profile), "Profile", "Device") == "SDL/0/Xbox 360 Controller" &&
               value(read(profile), "Profile", "Buttons/2") == "`Button S`",
           "the player's own per-game profile is moved, not replaced");
    expect(value(read(config / "Dolphin.ini"), "BluetoothPassthrough", "Enabled") == "False",
           "passthrough does not take the remotes over");
    expect(rewriteWritesNothing(setup), "a second launch of the game writes nothing");

    LaunchSetup next = setup;
    next.wiiRemote = remoteOf("ZZZE01");
    writeLaunchSettings(next);
    const std::string other = read(config / "WiimoteNew.ini");
    expect(value(other, "Wiimote1", "Device") == "SDL/0/Xbox 360 Controller" &&
               value(other, "Wiimote1", "Extension") == "Nunchuk" &&
               value(other, "Wiimote1", "Options/Sideways Wiimote") == "False" &&
               value(other, "Wiimote1", "IR/Up") == "`Right Y+`",
           "the next Wii game gets its own remote on the slot the last one left");
}

void testUnlistedWiiGame() {
    const WiiRemote remote = remoteOf("ZZZE01");
    const std::string run =
        dolphinSlotSettings(wiimotes, "Wiimote1", DolphinController::WiiRemote, 1, &remote);
    expect(value(run, "Wiimote1", "Extension") == "Nunchuk" &&
               value(run, "Wiimote1", "Options/Sideways Wiimote") == "False" &&
               value(run, "Wiimote1", "Nunchuk/Stick/Up") == "`Left Y+`",
           "a game not listed gets a remote and Nunchuk, the stick on the left stick");
    expect(value(run, "Wiimote1", "IR/Up") == "`Right Y+`" &&
               value(run, "Wiimote1", "IR/Relative Input") == "True",
           "and the pointer on the right stick");
    expect(value(run, "Wiimote1", "Source") == "1", "the slot's other settings are kept");
    expect(remoteOf("RMGP01").title == "Super Mario Galaxy",
           "a game is listed for every region by its first three characters");
    std::string error;
    expect(!WiiControls::parse(R"({"default": "a", "layouts": {"a": {"shak": ""}}, "games": {}})",
                               error) &&
               !error.empty(),
           "a misspelt key in the Wii controls is refused");
}

} // namespace

int main() try {
    testSlots();
    testLaunch();
    testOwnPads();
    testWiiGame();
    testUnlistedWiiGame();
    std::printf("dolphin_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
