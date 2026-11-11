// The scale a screen is read at where no window system gives one, and the glass an EDID states.
#include "display/recommended_scale.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "display/panel.hpp"
#include "support/test_main.hpp"

namespace {

namespace display = opensu::display;
using display::Panel;
using display::recommendedScale;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void television() {
    expect(recommendedScale({3840, 2160}, {}) == 2.5F, "4K of unknown size is 2.5");
    expect(recommendedScale({3840, 2160}, {.widthMm = 1210, .heightMm = 680}) == 2.5F,
           "a 55-inch 4K TV is 2.5");
    expect(recommendedScale({3840, 2160}, {.widthMm = 1872, .heightMm = 1053}) == 2.5F,
           "and so is an 85-inch one");
    expect(recommendedScale({2560, 1440}, {}) == 1.75F, "1440p of unknown size is 1.75");
    expect(recommendedScale({1920, 1080}, {}) == 1.25F, "1080p of unknown size is 1.25");
    expect(recommendedScale({1280, 800}, {}) == 1.0F, "1280x800 is 1");
    expect(recommendedScale({1280, 720}, {}) == 1.0F, "720p is 1");
    expect(recommendedScale({1080, 2400}, {}) == 1.25F, "a tall screen goes by its short side");
    expect(recommendedScale({7680, 4320}, {}) == 3.0F, "nothing passes 3");
    expect(recommendedScale({0, 0}, {}) == 1.0F, "no screen is 1");
}

void deskMonitor() {
    expect(recommendedScale({3840, 2160}, {.widthMm = 597, .heightMm = 336}) == 1.75F,
           "a 27-inch 4K monitor is 1.75");
    expect(recommendedScale({2560, 1440}, {.widthMm = 597, .heightMm = 336}) == 1.0F,
           "a 27-inch 1440p monitor is 1");
    expect(recommendedScale({1920, 1080}, {.widthMm = 531, .heightMm = 299}) == 1.0F,
           "a 24-inch 1080p monitor is 1");
}

void builtInPanel() {
    expect(recommendedScale({1280, 800}, {.widthMm = 100, .heightMm = 160, .builtIn = true}) ==
               1.5F,
           "a 7-inch 1280x800 handheld keeps 853 dp of width");
    expect(recommendedScale({1920, 1080}, {.widthMm = 155, .heightMm = 87, .builtIn = true}) ==
               2.25F,
           "a 7-inch 1080p handheld keeps 853x480 dp");
    expect(recommendedScale({1920, 1080}, {.widthMm = 344, .heightMm = 194, .builtIn = true}) ==
               1.0F,
           "a 15.6-inch 1080p laptop is 1");
    expect(recommendedScale({3840, 2160}, {.widthMm = 344, .heightMm = 194, .builtIn = true}) ==
               2.25F,
           "a 15.6-inch 4K laptop is 2.25");
}

std::array<std::uint8_t, 128> edid(int widthCm, int heightCm) {
    std::array<std::uint8_t, 128> block{};
    block[21] = static_cast<std::uint8_t>(widthCm);
    block[22] = static_cast<std::uint8_t>(heightCm);
    return block;
}

void edidGlass() {
    std::array<std::uint8_t, 128> block = edid(121, 68);
    Panel panel = display::panelOf(block, false);
    expect(panel.widthMm == 1210 && panel.heightMm == 680 && !panel.builtIn,
           "without a detailed timing the size is the centimetres");
    // 3840x2160 on 1872x1053 mm, as a TV states it.
    const std::array<std::uint8_t, 18> timing{0x08, 0xe8, 0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0,
                                              0x58, 0x8a, 0x00, 0x50, 0x1d, 0x74, 0x00, 0x00, 0x1e};
    std::copy(timing.begin(), timing.end(), block.begin() + 54);
    panel = display::panelOf(block, true);
    expect(panel.widthMm == 1872 && panel.heightMm == 1053 && panel.builtIn,
           "a detailed timing of the mode's shape gives the millimetres");
    // The same timing stating 4:3 glass is not believed.
    block[54 + 13] = 0x7c;
    block[54 + 14] = 0x75;
    panel = display::panelOf(block, false);
    expect(panel.widthMm == 1210 && panel.heightMm == 680,
           "a timing of another shape falls back to the centimetres");
    expect(!display::panelOf(std::array<std::uint8_t, 64>{}, false).known() &&
               !display::panelOf(edid(0, 0), false).known(),
           "a short or sizeless block is unknown");
}

} // namespace

int main() try {
    television();
    deskMonitor();
    builtInPanel();
    edidGlass();
    std::printf("recommended_scale: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
