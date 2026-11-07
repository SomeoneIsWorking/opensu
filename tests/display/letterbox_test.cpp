// A picture's place on screens of other shapes: centred, a wide one at the top of a tall screen.
#include "display/letterbox.hpp"

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

void wideScreen() {
    const display::Placed placed = display::letterbox({.width = 1600, .height = 900}, 4.0F / 3.0F);
    expect(placed.width == 1200 && placed.height == 900, "a 4:3 picture fills the height");
    expect(placed.x == 200 && placed.y == 0, "it stands centred across");
}

void tallScreen() {
    const display::Placed wide = display::letterbox({.width = 720, .height = 1560}, 4.0F / 3.0F);
    expect(wide.width == 720 && wide.height == 540, "a wide picture fills the width");
    expect(wide.x == 0 && wide.y == 0, "it stands at the top, the controls under it");
    const display::Placed tall = display::letterbox({.width = 720, .height = 1560}, 3.0F / 4.0F);
    expect(tall.width == 720 && tall.height == 960, "a vertical game fills the width");
    expect(tall.y == 300, "a vertical game stays centred");
}

} // namespace

int main() try {
    wideScreen();
    tallScreen();
    std::printf("letterbox: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
