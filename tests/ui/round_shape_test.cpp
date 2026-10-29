// Colour helpers against iiSU's theme backgrounds (docs/reference/iisu/home-grid.md §4).
#include "round_shape.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::test::expect;
using opensu::test::near;
using opensu::ui::Colour;
using opensu::ui::luminance;

void themeBackgrounds() {
    near(luminance(Colour{0, 0, 0, 255}), 0.0f, "black has no luminance");
    near(luminance(Colour{255, 255, 255, 255}), 1.0f, "white has full luminance");
    near(luminance(Colour{128, 128, 128, 255}), 0.2158605f, "mid grey is linearised", 1e-5f);
    // iiSU af8.e / af8.d backgrounds: tz5.q rgb(20,18,24) and tz5.x rgb(254,247,255).
    expect(luminance(Colour{20, 18, 24, 255}) < 0.5f, "the dark theme background is dark");
    expect(luminance(Colour{254, 247, 255, 255}) >= 0.5f, "the light theme background is light");
}

} // namespace

int main() try {
    themeBackgrounds();
    std::printf("round_shape: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
