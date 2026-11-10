// The GUID SDL gives a pad, against SDL 3's own for a uinput pad shaped like openSU's virtual pad.
#include "sdl_guid.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

int main() try {
    using opensu::test::expect;
    expect(opensu::gamepad::sdlGuid({.bus = 0x03,
                                     .vendor = 0x045e,
                                     .product = 0x028e,
                                     .version = 0x0110,
                                     .name = "opensu virtual pad"}) ==
               "0300cece5e0400008e02000010010000",
           "SDL_GUIDToString's GUID for the virtual pad");
    std::printf("sdl_guid: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
