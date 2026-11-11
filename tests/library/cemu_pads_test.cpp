// Cemu's controller profiles written bound to the virtual pads; a second launch writes nothing.
#include "library/cemu_pads.hpp"
#include "library/emulator_launch_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "support/launch_writes.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::LaunchSetup;
using opensu::library::reboundCemuProfile;
using opensu::library::writeLaunchSettings;
using opensu::test::expect;
using opensu::test::rewriteWritesNothing;

constexpr const char* guid = "0300cece5e0400008e02000010010000";

// Cemu 2.6's layout: a keyboard and an SDL pad on one emulated controller.
constexpr std::string_view profile = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                     "<emulated_controller>\n"
                                     "\t<type>Wii U Pro Controller</type>\n"
                                     "\t<controller>\n"
                                     "\t\t<api>Keyboard</api>\n"
                                     "\t\t<uuid>keyboard</uuid>\n"
                                     "\t\t<mappings />\n"
                                     "\t</controller>\n"
                                     "\t<controller>\n"
                                     "\t\t<api>SDLController</api>\n"
                                     "\t\t<uuid>0_050018dc5e040000130b000023050000</uuid>\n"
                                     "\t\t<display_name>Xbox Series X Controller</display_name>\n"
                                     "\t\t<mappings>\n"
                                     "\t\t\t<entry>\n"
                                     "\t\t\t\t<mapping>1</mapping>\n"
                                     "\t\t\t\t<button>0</button>\n"
                                     "\t\t\t</entry>\n"
                                     "\t\t</mappings>\n"
                                     "\t</controller>\n"
                                     "</emulated_controller>\n";

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void write(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

void testRebound() {
    const std::string bound = reboundCemuProfile(profile, std::string{"0_"} + guid);
    std::string wanted{profile};
    wanted.replace(wanted.find("0_050018dc"), 34, std::string{"0_"} + guid);
    expect(bound == wanted, "only the SDL controller's uuid changes");
    expect(reboundCemuProfile(bound, std::string{"0_"} + guid) == bound,
           "a profile already bound stays as it is");
    const std::string keyboard = "<controller><api>Keyboard</api><uuid>k</uuid></controller>";
    expect(reboundCemuProfile(keyboard, "0_x") == keyboard, "a keyboard is not rebound");
}

void testBind() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "cemu-pads"};
    const fs::path profiles = scratch.path() / "controllerProfiles";
    const std::string second = reboundCemuProfile(profile, "0_other");
    write(profiles / "controller0.xml", profile);
    write(profiles / "controller1.xml", second);
    write(profiles / "keyboard.xml", profile);
    const LaunchSetup setup{
        .emulatorId = "cemu", .config = scratch.path(), .data = {}, .padGuid = guid};
    writeLaunchSettings(setup);
    expect(read(profiles / "controller0.xml") ==
               reboundCemuProfile(profile, std::string{"0_"} + guid),
           "controller 0 plays the first virtual pad");
    expect(read(profiles / "controller1.xml") ==
               reboundCemuProfile(profile, std::string{"1_"} + guid),
           "controller 1 plays the second virtual pad");
    expect(read(profiles / "keyboard.xml") == profile, "a named profile is left alone");
    expect(!fs::exists(profiles / "controller2.xml"), "a controller the player lacks is not made");
    expect(rewriteWritesNothing(setup), "a second launch writes nothing");
}

} // namespace

int main() try {
    testRebound();
    testBind();
    std::printf("cemu_pads: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
