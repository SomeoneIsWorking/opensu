// The shipped launcher icons rasterise to a white, centred mask at the size asked for.
#include "vector_icon.hpp"

#include <cstdio>
#include <filesystem>

#include "check.hpp"

namespace {

using iideck::test::expect;

void testShippedIcons(const std::filesystem::path& assets) {
    for (const iideck::ui::Icon icon :
         {iideck::ui::Icon::Steam, iideck::ui::Icon::Epic, iideck::ui::Icon::Gog}) {
        const auto mask = iideck::ui::rasteriseMask(assets / iideck::ui::iconFile(icon), 48);
        expect(mask && mask->size() == 48u * 48u * 4u, "an icon rasterises at the size asked");
        std::size_t covered = 0;
        bool white = true;
        double sumX = 0.0;
        for (std::size_t i = 0; i < 48u * 48u; ++i) {
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
    expect(!iideck::ui::rasteriseMask("does/not/exist.svg", 32), "a missing file gives nothing");
    expect(!iideck::ui::rasteriseMask("does/not/exist.svg", 0), "no size gives nothing");
}

} // namespace

int main(int argc, char** argv) {
    expect(argc == 2, "usage: vector_icon_test ASSETS_DIR");
    testShippedIcons(argv[1]);
    testMissingFile();
    std::printf("vector_icon: all checks passed\n");
    return 0;
}
