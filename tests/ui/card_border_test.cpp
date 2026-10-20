// A console card's border colours, read off its top and bottom edges.
#include "card_border.hpp"

#include <cstdio>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;

void borders() {
    // A 32 x 32 card whose top stroke row is green and bottom stroke row red; the stroke's middle
    // is 13/1024 of the height, which is row 0 here.
    std::vector<std::uint8_t> card(std::size_t{32} * 32 * 4, 255);
    for (int x = 0; x < 32; ++x) {
        const std::size_t top = static_cast<std::size_t>(x) * 4;
        const std::size_t bottom = (std::size_t{31} * 32 + static_cast<std::size_t>(x)) * 4;
        card[top] = 0;
        card[top + 1] = 200;
        card[top + 2] = 0;
        card[bottom] = 240;
        card[bottom + 1] = 20;
        card[bottom + 2] = 60;
    }
    const opensu::ui::Gradient gradient = opensu::ui::borderColours(card, 32, 32);
    expect(gradient.from.r == 0 && gradient.from.g == 200, "the top edge's colour is the start");
    expect(gradient.to.r == 240 && gradient.to.b == 60, "the bottom edge's is the end");
    const opensu::ui::Gradient none =
        opensu::ui::borderColours(std::span<const std::uint8_t>{}, 32, 32);
    expect(none.from.r == 0 && none.to.r == 0, "too few pixels read as nothing");
}

} // namespace

int main() {
    borders();
    std::printf("card_border: all checks passed\n");
    return 0;
}
