// Recolouring the gamepad icon: white stays, the gradient takes the console's colours.
#include "icon_recolour.hpp"

#include <cstdio>
#include <vector>

#include "check.hpp"

namespace {

using iideck::test::expect;
using iideck::ui::Rgb;

void recolour() {
    // 2 x 2: a white pixel, a cyan one, a dark blue one (the base), and a half-covered white one.
    std::vector<std::uint8_t> pixels{255, 255, 255, 255, 64, 240, 255, 255,
                                     255, 255, 255, 90,  0,  60,  140, 255};
    iideck::ui::recolourIcon(pixels, 2, 2, Rgb{0, 200, 0}, Rgb{255, 0, 0});
    expect(pixels[0] == 255 && pixels[1] == 255 && pixels[2] == 255, "white stays white");
    expect(pixels[3] == 255 && pixels[7] == 255 && pixels[11] == 90 && pixels[15] == 255,
           "alpha is untouched");
    expect(pixels[4] < 40 && pixels[5] > 170, "the cyan top becomes the gradient's start, green");
    expect(pixels[12] > 100 && pixels[12] < 200 && pixels[13] < 30,
           "the dark base is the gradient's end, red, and darker than the face would be");
}

void borders() {
    // A 32 x 32 card whose top stroke row is green and bottom stroke row red; the stroke's middle
    // is 13/1024 of the height, which is row 0 here.
    std::vector<std::uint8_t> card(32 * 32 * 4, 255);
    for (int x = 0; x < 32; ++x) {
        const std::size_t top = static_cast<std::size_t>(x) * 4;
        const std::size_t bottom = (31 * 32 + static_cast<std::size_t>(x)) * 4;
        card[top] = 0;
        card[top + 1] = 200;
        card[top + 2] = 0;
        card[bottom] = 240;
        card[bottom + 1] = 20;
        card[bottom + 2] = 60;
    }
    const iideck::ui::Gradient gradient = iideck::ui::borderColours(card, 32, 32);
    expect(gradient.from.r == 0 && gradient.from.g == 200, "the top edge's colour is the start");
    expect(gradient.to.r == 240 && gradient.to.b == 60, "the bottom edge's is the end");
    const iideck::ui::Gradient none =
        iideck::ui::borderColours(std::span<const std::uint8_t>{}, 32, 32);
    expect(none.from.r == 0 && none.to.r == 0, "too few pixels read as nothing");
}

void empty() {
    std::vector<std::uint8_t> none;
    iideck::ui::recolourIcon(none, 0, 0, Rgb{}, Rgb{});
    expect(none.empty(), "nothing to recolour");
}

} // namespace

int main() {
    recolour();
    borders();
    empty();
    std::printf("icon_recolour: all checks passed\n");
    return 0;
}
