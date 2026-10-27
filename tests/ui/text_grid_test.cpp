// Labels on the pixel grid, laid out from the shipped face the way Typeface lays them out.
#include "text_grid.hpp"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;
using opensu::test::near;
using opensu::ui::FaceMetrics;
using opensu::ui::firstCodepoint;
using opensu::ui::TextGrid;
using opensu::ui::TextStyle;

FaceMetrics calSans(const char* path) {
    std::ifstream stream{path, std::ios::binary};
    const std::vector<char> bytes{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
    auto metrics = opensu::ui::readFaceMetrics(std::as_bytes(std::span{bytes}));
    if (!metrics) {
        fail("the shipped face reads");
    }
    return std::move(*metrics);
}

void wholePixelBaselineExactAdvances(const FaceMetrics& face) {
    // Settings' value text at 1280x800: 15.5 sp at 1280 / 853 px per dp, a 30.24 px line box.
    const TextGrid grid{face, TextStyle{15.5f * 1280.0f / 853.0f}};
    const float stretch = grid.lineBox() / 30.0f;
    near(grid.lineBox(), 30.24f, "the style's line box", 0.01);
    near(grid.em(), 30.0f / 1.3f, "glyphs are rasterised at the em of a 30 px line box");
    // 'o' is 594 units of a 1300-unit line box.
    near(grid.advance(U'o'), 594.0f * grid.lineBox() / 1300.0f, "an advance is exact", 0.001);
    // hhea ascent 1000 of 1300: 23.08 px, cut to 23.
    near(grid.ascent(), 23.0f * stretch, "the baseline is cut to whole pixels");
    // 5760 units, the space 164 of them.
    near(grid.measure("None hidden"), 5760.0f * grid.lineBox() / 1300.0f,
         "a label is its exact advances", 0.01);
}

void tracking(const FaceMetrics& face) {
    const TextGrid plain{face, TextStyle{20.0f}};
    const TextGrid spaced{face, TextStyle{20.0f, 0.1f}};
    near(spaced.lead(), 1.0f, "half the spacing comes before the first glyph");
    near(spaced.advance(U'a') - plain.advance(U'a'), 2.0f, "each glyph adds the spacing");
    near(spaced.measure("ab") - plain.measure("ab"), 4.0f, "a label adds it per glyph");
}

void decodesUtf8() {
    expect(firstCodepoint("\xC3\xA9t\xC3\xA9") == std::pair<char32_t, std::size_t>{U'é', 2},
           "a two-byte character");
    expect(firstCodepoint("\xE2\x80\xA6") == std::pair<char32_t, std::size_t>{U'…', 3},
           "a three-byte character");
    expect(firstCodepoint("\xFF"
                          "a") == std::pair<char32_t, std::size_t>{U'�', 1},
           "a stray byte reads as one replacement character");
    expect(firstCodepoint("\xC3") == std::pair<char32_t, std::size_t>{U'�', 1},
           "a cut-off character reads as one replacement character");
}

} // namespace

int main(int argc, char** argv) {
    expect(argc == 2, "usage: text_grid_test FONT");
    const FaceMetrics face = calSans(argv[1]);
    wholePixelBaselineExactAdvances(face);
    tracking(face);
    decodesUtf8();
    std::printf("text_grid: all checks passed\n");
    return 0;
}
