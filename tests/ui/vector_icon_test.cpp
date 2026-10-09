// The shipped launcher icons rasterise to a white, centred mask at the size asked for.
#include "vector_icon.hpp"

#include <cstdio>
#include <filesystem>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;

void testShippedIcons(const std::filesystem::path& assets) {
    for (const opensu::ui::Icon icon :
         {opensu::ui::Icon::Steam, opensu::ui::Icon::Epic, opensu::ui::Icon::Gog}) {
        const auto mask = opensu::ui::rasteriseMask(assets / opensu::ui::iconFile(icon), 48);
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

void testMissingFile() {
    expect(!opensu::ui::rasteriseMask("does/not/exist.svg", 32), "a missing file gives nothing");
    expect(!opensu::ui::rasteriseMask("does/not/exist.svg", 0), "no size gives nothing");
}

} // namespace

int main(int argc, char** argv) {
    expect(argc == 2, "usage: vector_icon_test ASSETS_DIR");
    testShippedIcons(argv[1]);
    testMissingFile();
    std::printf("vector_icon: all checks passed\n");
    return 0;
}
