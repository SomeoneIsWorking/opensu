// A colour temperature as Gamescope's night mode: the blackbody white point at the luminance of
// white, and the HSV that gives the same white point back.
#include "session/night_mode.hpp"
#include "support/test_main.hpp"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

using opensu::session::nightModeOf;
using opensu::session::whitePointOf;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-5F;
}

} // namespace

int main() try {
    expect(whitePointOf(6500) == std::array{1.0F, 1.0F, 1.0F} &&
               whitePointOf(9000) == std::array{1.0F, 1.0F, 1.0F},
           "6500 K and above is white");
    const auto at4500 = whitePointOf(4500);
    expect(at4500[0] > 1.0F && at4500[1] < at4500[0] && at4500[2] < at4500[1],
           "a warm white point lifts red past full and takes more blue than green");
    const auto linear = [](float encoded) {
        return std::pow(encoded, 2.2F);
    };
    for (const int kelvin : {1000, 2500, 4500, 6000}) {
        const auto white = whitePointOf(kelvin);
        const float luminance =
            0.2126F * linear(white[0]) + 0.7152F * linear(white[1]) + 0.0722F * linear(white[2]);
        expect(std::fabs(luminance - 1.0F) < 1e-4F, "a warm white point keeps white's luminance");
    }
    bool cooling = true;
    for (int kelvin = 1100; kelvin <= 6500; kelvin += 100) {
        const auto cooler = whitePointOf(kelvin);
        const auto warmer = whitePointOf(kelvin - 100);
        cooling = cooling && cooler[0] < warmer[0] &&
                  cooler[1] / cooler[0] > warmer[1] / warmer[0] &&
                  cooler[2] / cooler[0] >= warmer[2] / warmer[0];
    }
    expect(cooling, "red falls and green and blue come back as the temperature rises");
    expect(whitePointOf(1000)[2] == 0.0F && whitePointOf(500) == whitePointOf(1000),
           "1000 K has no blue, and below it is 1000 K");

    expect(nightModeOf(6500).amount == 0.0F && nightModeOf(7000).amount == 0.0F,
           "neutral is no night mode");
    const opensu::session::NightMode mode = nightModeOf(4500);
    expect(mode.amount == 1.0F, "a warm temperature is the night mode at full amount");
    // HSV with the red channel largest: red is v, blue v (1 - s), green v (1 - s (1 - 6h)).
    expect(near(mode.value, at4500[0]) && near(mode.value * (1.0F - mode.saturation), at4500[2]) &&
               near(mode.value * (1.0F - mode.saturation * (1.0F - 6.0F * mode.hue)), at4500[1]),
           "the hue, saturation and value give 4500 K's white point back");
    const auto atom = mode.atom();
    expect(std::bit_cast<float>(atom[0]) == 1.0F && std::bit_cast<float>(atom[1]) == mode.hue &&
               std::bit_cast<float>(atom[2]) == mode.saturation &&
               std::bit_cast<float>(atom[3]) == mode.value,
           "the atom is amount, hue, saturation and value as float bits");

    std::printf("night_mode: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
