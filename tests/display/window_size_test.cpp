// The window a dp-sized interface opens in: scaled by the display, kept on its usable area.
#include "display/window_size.hpp"

#include <cstdio>
#include <cstdlib>

#include "support/test_main.hpp"

namespace {

namespace display = opensu::display;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void plainDisplay() {
    const display::Size window = display::windowOn({1280, 800}, 1.0F, {1920, 1040});
    expect(window.width == 1280 && window.height == 800, "1 per dp opens the dp size");
}

void scaledDisplay() {
    const display::Size window = display::windowOn({1280, 800}, 2.5F, {3840, 2100});
    expect(window.width == 3200 && window.height == 2000, "2.5 per dp opens 3200x2000");
    const display::Size least = display::windowOn({960, 600}, 2.5F, {window.width, window.height});
    expect(least.width == 2400 && least.height == 1500, "the minimum scales with it");
}

void smallDisplay() {
    const display::Size window = display::windowOn({1280, 800}, 2.0F, {1920, 1040});
    expect(window.width == 1920 && window.height == 1040, "it stays on the usable area");
    const display::Size least = display::windowOn({960, 600}, 2.0F, {window.width, window.height});
    expect(least.width == 1920 && least.height == 1040, "the minimum never passes the window");
}

void unknownDisplay() {
    const display::Size window = display::windowOn({1280, 800}, 0.0F, {0, 0});
    expect(window.width == 1280 && window.height == 800, "an unread display changes nothing");
}

} // namespace

int main() try {
    plainDisplay();
    scaledDisplay();
    smallDisplay();
    unknownDisplay();
    std::printf("window_size: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
