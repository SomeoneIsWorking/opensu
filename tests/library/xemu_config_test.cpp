// xemu.toml as a launch reads it: the welcome window off, every port on the virtual pads' GUID.
#include "library/xemu_config.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "library/ini_file.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::library::IniKey;
using opensu::library::iniValueIn;
using opensu::library::xemuLaunchSettings;
using opensu::test::expect;

constexpr std::string_view guid{"0300cece5e0400008e02000010010000"};
constexpr std::string_view quoted{"'0300cece5e0400008e02000010010000'"};

// Port 1 on the keyboard, port 2 on another pad, the rest unbound.
constexpr std::string_view player = "[general]\n"
                                    "show_welcome = true\n"
                                    "\n"
                                    "[input]\n"
                                    "auto_bind = false\n"
                                    "\n"
                                    "[input.bindings]\n"
                                    "port1 = 'keyboard'\n"
                                    "port2 = '03000000de280000ff11000001000000'\n";

std::string value(std::string_view toml, const IniKey& key) {
    return iniValueIn(toml, key).value_or("<none>");
}

void testWelcome() {
    const std::string run = xemuLaunchSettings(player, "");
    expect(value(run, {"general", "show_welcome"}) == "false", "the welcome window is off");
    expect(value(run, {"input.bindings", "port1"}) == "'keyboard'",
           "without virtual pads the ports stay");
    expect(xemuLaunchSettings(run, "") == run, "a file already set stays byte for byte");
}

void testPorts() {
    const std::string run = xemuLaunchSettings(player, guid);
    for (const char* port : {"port1", "port2", "port3", "port4"}) {
        expect(value(run, {"input.bindings", port}) == quoted,
               "every port lists the virtual pads' GUID, so pad N takes port N");
    }
    expect(value(run, {"input", "auto_bind"}) == "false", "the player's other settings stay");
    const std::string fresh = xemuLaunchSettings("", guid);
    expect(value(fresh, {"input.bindings", "port1"}) == quoted &&
               value(fresh, {"general", "show_welcome"}) == "false",
           "a player with no settings file gets the virtual pads and no welcome window");
    expect(xemuLaunchSettings(run, guid) == run, "a file already bound stays as it is");
}

} // namespace

int main() try {
    testWelcome();
    testPorts();
    std::printf("xemu_config: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
