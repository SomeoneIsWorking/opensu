// A held direction: one move, a pause, then steady repeats.
#include "direction_repeat.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::gamepad::Button;
using opensu::gamepad::DirectionRepeat;
using opensu::test::expect;
using namespace std::chrono_literals;

/// How many moves `repeat` gives over `span`, polled every frame at 60 Hz.
int movesOver(DirectionRepeat& repeat, DirectionRepeat::Clock::time_point from,
              std::chrono::milliseconds span) {
    int moves = 0;
    for (auto t = from; t <= from + span; t += 16ms) {
        if (repeat.poll(t)) {
            ++moves;
        }
    }
    return moves;
}

} // namespace

int main() {
    const DirectionRepeat::Clock::time_point start{};

    DirectionRepeat tap;
    tap.press(Button::Down, start);
    expect(tap.poll(start) == Button::Down, "a press moves at once");
    tap.release(Button::Down);
    expect(movesOver(tap, start, 2000ms) == 0, "a released direction stops");

    DirectionRepeat quick;
    quick.press(Button::Right, start);
    quick.release(Button::Right);
    expect(quick.poll(start + 16ms) == Button::Right,
           "a tap released before the frame polls still moves once");
    expect(movesOver(quick, start + 16ms, 2000ms) == 0, "and only once");

    DirectionRepeat held;
    held.press(Button::Right, start);
    expect(movesOver(held, start, 390ms) == 1, "a held direction waits before repeating");
    expect(movesOver(held, start + 400ms, 590ms) == 6, "then moves every 100 ms");

    DirectionRepeat roll;
    roll.press(Button::Left, start);
    expect(roll.poll(start) == Button::Left, "left moves");
    roll.press(Button::Up, start + 50ms);
    expect(roll.poll(start + 50ms) == Button::Up, "a new direction moves at once");
    roll.release(Button::Left);
    expect(!roll.poll(start + 60ms), "releasing the old direction leaves the new one waiting");

    expect(DirectionRepeat::repeats(Button::Up) && !DirectionRepeat::repeats(Button::A),
           "only directions repeat");
    std::printf("direction_repeat: all checks passed\n");
    return 0;
}
