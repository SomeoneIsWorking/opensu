// The real Wii Remotes from a fake /sys/bus/hid/devices and /dev: a DolphinBar in Wii Remote mode
// (four USB slots of one device), the bar in its gamepad mode, a Bluetooth remote, other HID
// devices ignored, and slots this user cannot open.
#include "device/wii_remotes.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::device::findWiiRemotes;
using opensu::device::HidPlaces;
using opensu::device::WiiRemotes;
using opensu::test::expect;

HidPlaces freshPlaces(const char* name) {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "wii-remotes" / name;
    fs::remove_all(root);
    fs::create_directories(root / "devices");
    fs::create_directories(root / "dev");
    return {.devices = root / "devices", .dev = root / "dev"};
}

/// A HID device folder as the kernel writes it, with its hidraw node when `hidraw` is named.
void hid(const HidPlaces& places, const std::string& folder, const std::string& id,
         const std::string& phys, const std::string& hidraw = {}) {
    const fs::path dir = places.devices / folder;
    fs::create_directories(dir);
    std::ofstream{dir / "uevent"} << "DRIVER=hid-generic\nHID_ID=" << id
                                  << "\nHID_NAME=Some Device\nHID_PHYS=" << phys
                                  << "\nHID_UNIQ=\nMODALIAS=hid:b0003\n";
    if (!hidraw.empty()) {
        fs::create_directories(dir / "hidraw" / hidraw);
        std::ofstream{places.dev / hidraw} << "";
    }
}

void dolphinBar(const HidPlaces& places) {
    for (int slot = 0; slot < 4; ++slot) {
        const std::string n = std::to_string(slot);
        hid(places, "0003:057E:0306.000" + n, "0003:0000057E:00000306",
            "usb-0000:00:14.0-2/input" + n, "hidraw" + n);
    }
}

void testNone() {
    const HidPlaces places = freshPlaces("none");
    hid(places, "0003:046D:C548.0001", "0003:0000046D:0000C548", "usb-0000:00:14.0-1/input0",
        "hidraw9");
    expect(findWiiRemotes(places) == WiiRemotes{}, "a mouse receiver is no Wii Remote");
    expect(!findWiiRemotes(places).present(), "and no real remote is there");
    expect(findWiiRemotes({.devices = places.devices / "missing", .dev = places.dev}) ==
               WiiRemotes{},
           "a machine without HID devices has none");
}

void testDolphinBar() {
    const HidPlaces places = freshPlaces("bar");
    dolphinBar(places);
    const WiiRemotes found = findWiiRemotes(places);
    expect(found.dolphinBars == 1 && found.barsReachable && found.bluetoothRemotes == 0,
           "a DolphinBar in Wii Remote mode is one bar of four slots");
    expect(found.present(), "and Wii games play real remotes on it");
}

void testOtherMode() {
    const HidPlaces places = freshPlaces("other-mode");
    hid(places, "0003:0079:1803.0004", "0003:00000079:00001803", "usb-0000:00:14.0-2/input0");
    const WiiRemotes found = findWiiRemotes(places);
    expect(found.otherModeBars == 1 && found.dolphinBars == 0,
           "a DolphinBar in its gamepad mode is seen");
    expect(!found.present(), "but gives Wii games no real remote");
}

void testBluetooth() {
    const HidPlaces places = freshPlaces("bluetooth");
    hid(places, "0005:057E:0330.0007", "0005:0000057E:00000330", "00:1a:7d:da:71:13", "hidraw4");
    hid(places, "0005:057E:0306.0008", "0005:0000057E:00000306", "00:1a:7d:da:71:13", "hidraw5");
    const WiiRemotes found = findWiiRemotes(places);
    expect(found.bluetoothRemotes == 2 && found.dolphinBars == 0,
           "each remote connected over Bluetooth counts, either model");
    expect(found.present(), "and Wii games play them");
}

void testUnreachable() {
    const HidPlaces places = freshPlaces("unreachable");
    dolphinBar(places);
    fs::permissions(places.dev / "hidraw2", fs::perms::owner_read);
    const WiiRemotes found = findWiiRemotes(places);
    expect(found.dolphinBars == 1 && !found.barsReachable,
           "a slot this user cannot write is a bar Dolphin cannot use");
    fs::permissions(places.dev / "hidraw2", fs::perms::owner_read | fs::perms::owner_write);
}

void testHiddenRun() {
    const HidPlaces hidden = HidPlaces::of("/data", true);
    expect(hidden.devices == "/data/hidden-hid/devices" && hidden.dev == "/data/hidden-hid/dev",
           "a hidden run reads only its own fake devices");
    expect(HidPlaces::of("/data", false).devices == "/sys/bus/hid/devices",
           "another run reads the system's");
}

} // namespace

int main() try {
    testNone();
    testDolphinBar();
    testOtherMode();
    testBluetooth();
    testUnreachable();
    testHiddenRun();
    std::printf("wii_remotes: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
