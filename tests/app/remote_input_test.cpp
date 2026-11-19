// The keys remote play sends: a shortcut this openSU acted on, and a key held when a menu took the
// controls, stay here until let go, so the Shift-Tab that opens or closes the Guide never reaches
// the host.
#include "remote_input.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using opensu::app::RemoteInput;
using opensu::test::expect;

constexpr std::uint8_t shift = 225;
constexpr std::uint8_t tab = 43;
constexpr std::uint8_t letterA = 4;

std::vector<std::uint8_t> sent(RemoteInput& input, const std::vector<std::uint8_t>& held) {
    input.keys(held);
    return input.take().keys.held;
}

} // namespace

int main() try {
    RemoteInput input;
    expect(sent(input, {shift}) == std::vector{shift}, "a key pressed in play goes to the host");

    // Shift-Tab opens this openSU's Guide, in a frame the host's game still has the controls.
    input.keepKeys();
    expect(sent(input, {shift, tab}).empty(), "the shortcut that opened the Guide stays here");
    // The menu has the controls.
    input.release();
    expect(input.take().keys.held.empty(), "a menu's frame sends no keys");
    // Shift-Tab again closes it, and the controls come back with both still held.
    expect(sent(input, {shift, tab}).empty(), "the keys that closed the menu stay here");
    expect(sent(input, {shift, tab, letterA}) == std::vector{letterA},
           "a key pressed after the menu goes to the host");
    expect(sent(input, {letterA}) == std::vector{letterA},
           "letting the kept keys go sends nothing");
    expect(sent(input, {letterA, tab}) == (std::vector{letterA, tab}),
           "a kept key pressed again goes to the host");
    std::printf("remote_input: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
