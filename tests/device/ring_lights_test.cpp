// RingLights against a fake /sys/class/leds: the Ally's packed zones and the red/green/blue layout.
#include "device/ring_lights.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace {

namespace fs = std::filesystem;
using opensu::device::RingColour;
using opensu::device::RingLights;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path freshRoot(const char* name) {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "ring_lights" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

fs::path led(const fs::path& root, const char* name, const char* index, const char* max) {
    const fs::path dir = root / name;
    fs::create_directories(dir);
    std::ofstream{dir / "multi_index"} << index << '\n';
    std::ofstream{dir / "max_brightness"} << max << '\n';
    std::ofstream{dir / "multi_intensity"} << "0\n";
    std::ofstream{dir / "brightness"} << "0\n";
    return dir;
}

std::string contents(const fs::path& file) {
    std::ifstream in{file};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void allyZonesArePacked() {
    const fs::path root = freshRoot("ally");
    led(root, "input5::capslock", "", "1");
    const fs::path dir = led(root, "ally:rgb:joystick_rings", "rgb rgb rgb rgb", "255");
    const std::optional<RingLights> lights = RingLights::find(root);
    expect(lights && lights->zones() == 4, "the Ally's rings are four packed zones");
    expect(lights->set(RingColour{255, 96, 0}, 50).empty(), "the colour is written");
    expect(contents(dir / "multi_intensity") == "16736256 16736256 16736256 16736256",
           "each zone holds 0xRRGGBB");
    expect(contents(dir / "brightness") == "127", "50 percent of 255");
    expect(lights->set({}, 0).empty() && contents(dir / "brightness") == "0", "0 turns them off");
}

void separateChannels() {
    const fs::path root = freshRoot("legion");
    const fs::path dir = led(root, "go:rgb:joystick_rings", "red green blue", "100");
    const std::optional<RingLights> lights = RingLights::find(root);
    expect(lights && lights->zones() == 1, "red green blue is one zone");
    expect(lights->set(RingColour{0, 255, 160}, 100).empty(), "the colour is written");
    expect(contents(dir / "multi_intensity") == "0 255 160", "one value per channel");
    expect(contents(dir / "brightness") == "100", "full brightness is the LED's maximum");
}

void noneWithoutTheLed() {
    const fs::path root = freshRoot("none");
    led(root, "xpad0", "", "255");
    led(root, "odd:rgb:joystick_rings", "rgbw", "255");
    expect(!RingLights::find(root), "no ring LED, or one of an unknown layout, is none");
    expect(!RingLights::find(root / "missing"), "no LED class, none");
}

} // namespace

int main() try {
    allyZonesArePacked();
    separateChannels();
    noneWithoutTheLed();
    std::printf("ring_lights: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
