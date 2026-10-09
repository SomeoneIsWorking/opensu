// The bell spinner's arc against compose-material3's indeterminate keyframes.
#include "progress_spinner.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.01f;
}

float wrap(float degrees) {
    return std::fmod(std::fmod(degrees, 360.0f) + 360.0f, 360.0f);
}

} // namespace

int main() {
    using opensu::ui::spinnerArc;
    const auto start = spinnerArc(0.0);
    expect(near(start.start, -90.0f) && near(start.sweep, 0.0f), "it starts empty at 12 o'clock");

    const auto half = spinnerArc(0.666);
    expect(near(half.sweep, 290.0f), "the head has jumped 290 degrees by half a rotation");
    expect(near(half.start, -90.0f + 143.0f), "the base turns 286 degrees per rotation");

    const auto mid = spinnerArc(0.333);
    expect(mid.sweep > 145.0f, "the head eases out fast");

    const auto before = spinnerArc(1.3319);
    const auto after = spinnerArc(1.332);
    expect(near(after.sweep, 0.0f) && near(wrap(after.start), wrap(-90.0f + 216.0f)),
           "each rotation starts 216 degrees on");
    expect(std::abs(wrap(before.start + before.sweep) - wrap(after.start)) < 0.5f,
           "the arc is continuous across a rotation");

    const auto cycle = spinnerArc(1.332 * 5);
    expect(near(cycle.start, start.start) && near(cycle.sweep, start.sweep),
           "five rotations make a cycle");
    std::printf("progress spinner: all checks passed\n");
    return 0;
}
