// The shipped launcher icons rasterise to a white, centred mask at the size asked for; the dock's
// own icons in their colours, fitted to the box, with paints swapped on request.
#include "vector_icon.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;

void testShippedIcons(const std::filesystem::path& assets) {
    for (const opensu::ui::Icon icon :
         {opensu::ui::Icon::Steam, opensu::ui::Icon::Epic, opensu::ui::Icon::Gog}) {
        const auto mask = opensu::svg::rasteriseMask(assets / opensu::ui::iconFile(icon), 48);
        if (!mask) {
            fail("an icon rasterises at the size asked");
        }
        expect(mask->size() == std::size_t{48} * 48 * 4, "an icon rasterises at the size asked");
        std::size_t covered = 0;
        bool white = true;
        double sumX = 0.0;
        for (std::size_t i = 0; i < std::size_t{48} * 48; ++i) {
            const unsigned char* px = &(*mask)[i * 4];
            white = white && px[0] == 255 && px[1] == 255 && px[2] == 255;
            if (px[3] > 127) {
                ++covered;
                sumX += static_cast<double>(i % 48);
            }
        }
        expect(white, "the mask is white, for tinting");
        expect(covered > 48u * 48u / 10u, "the mark covers a real share of the square");
        const double meanX = sumX / static_cast<double>(covered);
        expect(meanX > 12.0 && meanX < 36.0, "the mark sits around the centre");
    }
}

using opensu::ui::Icon;

/// The pixel at (`x`, `y`) of a `width`-wide RGBA image.
const unsigned char* pixelAt(const std::vector<unsigned char>& rgba, int width, int x, int y) {
    return &rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(x)) *
                 4];
}

void testDockIcons(const std::filesystem::path& assets) {
    using opensu::library::Section;
    expect(opensu::ui::dockIcon(Section::Home, false) == Icon::Home &&
               opensu::ui::dockIcon(Section::Home, true) == Icon::HomeSelected &&
               opensu::ui::dockIcon(Section::Library, false) == Icon::Library &&
               opensu::ui::dockIcon(Section::Library, true) == Icon::LibrarySelected,
           "each section has an icon for either state");
    for (const Icon icon : {Icon::Home, Icon::HomeSelected, Icon::Library, Icon::LibrarySelected}) {
        // A wide box: the icon fills its height and sits centred across.
        constexpr int width = 90;
        constexpr int height = 40;
        const auto rgba =
            opensu::svg::rasterise(assets / opensu::ui::iconFile(icon), width, height);
        if (!rgba) {
            fail("a dock icon rasterises");
        }
        expect(rgba->size() == std::size_t{width} * height * 4, "at the size asked");
        int left = width;
        int right = -1;
        int top = height;
        int bottom = -1;
        bool coloured = false;
        bool whiteFace = false;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const unsigned char* px = pixelAt(*rgba, width, x, y);
                if (px[3] < 128) {
                    continue;
                }
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                coloured = coloured || px[0] != px[2];
                whiteFace = whiteFace || (px[0] == 255 && px[1] == 255 && px[2] == 255);
            }
        }
        expect(top <= 1 && bottom >= height - 2, "it fills the box's height");
        expect(std::abs((left + right) - (width - 1)) <= 2, "it is centred across");
        expect(coloured && whiteFace, "a coloured body round a white face");
    }
}

void testSwaps(const std::filesystem::path& assets) {
    constexpr int side = 64;
    const std::filesystem::path svg = assets / opensu::ui::iconFile(Icon::Library);
    const opensu::svg::ColourSwap toRed[] = {
        {opensu::ui::libraryTop, {255, 0, 0, 255}},
        {opensu::ui::libraryBottom, {255, 0, 0, 255}},
        {opensu::ui::libraryEdge, {255, 0, 0, 255}},
    };
    const auto own = opensu::svg::rasterise(svg, side, side);
    const auto red = opensu::svg::rasterise(svg, side, side, toRed);
    if (!own || !red) {
        fail("the Library icon rasterises");
    }
    bool ownBlue = false;
    bool allRed = true;
    for (std::size_t i = 0; i < own->size(); i += 4) {
        const unsigned char* mine = &(*own)[i];
        const unsigned char* swapped = &(*red)[i];
        // Body pixels only: the white face and the edges blended into it are not swapped.
        if (mine[3] < 255 || std::min({mine[0], mine[1], mine[2]}) >= 100) {
            continue;
        }
        ownBlue = ownBlue || mine[2] > mine[0];
        allRed = allRed && swapped[0] > 200 && swapped[1] < 90 && swapped[2] < 90;
    }
    expect(ownBlue, "the icon's own body is drawn in its blues");
    expect(allRed, "with every one of its colours swapped, its whole body is the new colour");
}

void testMissingFile() {
    expect(!opensu::svg::rasteriseMask("does/not/exist.svg", 32), "a missing file gives nothing");
    expect(!opensu::svg::rasteriseMask("does/not/exist.svg", 0), "no size gives nothing");
}

} // namespace

int main(int argc, char** argv) try {
    expect(argc == 2, "usage: vector_icon_test ASSETS_DIR");
    testShippedIcons(argv[1]);
    testDockIcons(argv[1]);
    testSwaps(argv[1]);
    testMissingFile();
    std::printf("vector_icon: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
