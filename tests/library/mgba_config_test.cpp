// mGBA's config.ini as a launch writes it: the virtual pads' input profile completed from mGBA's
// own xpad profile, the player's bindings kept; a second launch writes nothing.
#include "library/mgba_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
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
using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::LaunchSetup;
using opensu::library::mgbaLaunchSettings;
using opensu::library::writeLaunchSettings;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;

constexpr const char* guid = "0300cece5e0400008e02000010010000";
constexpr std::string_view section = "gba.input-profile.Xbox 360 Controller";

// mGBA 0.10.5's own file: window settings, and a profile the player started for the pad.
constexpr std::string_view player = "[ports.qt]\n"
                                    "fullscreen=0\n"
                                    "\n"
                                    "[gba.input-profile.Xbox 360 Controller]\n"
                                    "keyA=2\n";

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void write(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

std::optional<std::string> value(std::string_view text, std::string_view key) {
    return iniValueIn(text, IniKey{.section = section, .key = key});
}

void testProfile() {
    const std::string fresh = mgbaLaunchSettings("");
    expect(value(fresh, "keyA") == "1" && value(fresh, "keyB") == "0" &&
               value(fresh, "keyStart") == "7" && value(fresh, "keySelect") == "6" &&
               value(fresh, "keyL") == "4" && value(fresh, "keyR") == "5",
           "the face, shoulder, Start and Select buttons are mGBA's own xpad profile's");
    expect(value(fresh, "hat0Up") == "6" && value(fresh, "axisLeftAxis") == "-0",
           "the d-pad and the stick are bound too");
    const std::string kept = mgbaLaunchSettings(player);
    expect(value(kept, "keyA") == "2" && value(kept, "keyB") == "0",
           "a button the player bound is kept, the rest added");
    expect(iniValueIn(kept, IniKey{.section = "ports.qt", .key = "fullscreen"}) == "0",
           "the player's other settings are kept");
    expect(mgbaLaunchSettings(kept) == kept, "a complete profile is left as it is");
}

void testLaunch() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "mgba-config"};
    const fs::path file = scratch.path() / "config.ini";
    write(file, player);
    const LaunchSetup own{
        .emulatorId = "mgba", .config = scratch.path(), .data = {}, .padGuid = {}};
    writeLaunchSettings(own);
    expect(read(file) == player, "a game reading the pads themselves keeps the player's");
    const LaunchSetup setup{
        .emulatorId = "mgba", .config = scratch.path(), .data = {}, .padGuid = guid};
    writeLaunchSettings(setup);
    expect(read(file) == mgbaLaunchSettings(player), "the completed profile is written");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testProfile();
    testLaunch();
    std::printf("mgba_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
