// Tile motion against iiSU's curves (docs/reference/iisu/motion.md §1, §4).
#include "tile_motion.hpp"

#include <cmath>
#include <cstdio>

#include "check.hpp"

namespace {

using iideck::test::expect;
using iideck::test::near;
namespace motion = iideck::ui::motion;

void focus() {
    near(motion::focusScale(0.0f), 1.0f, "focus starts at 1.00");
    near(motion::focusScale(72.5f), 1.04375f, "half way up is ease-out cubic");
    near(motion::focusScale(145.0f), 1.05f, "peaks at 1.05 after 145 ms");
    near(motion::focusScale(205.0f), 1.0325f, "settling is ease-out cubic too");
    near(motion::focusScale(265.0f), 1.03f, "rests at 1.03 after 265 ms");
    near(motion::focusScale(400.0f), 1.03f, "holds 1.03");
}

void domino() {
    expect(motion::dominoStep(iideck::ui::GridCell{2, 1, 2, 1, 0}) == 1,
           "the wave step is column minus row");
    motion::Entrance e = motion::domino(0.0f, 0);
    near(e.alpha, 0.0f, "the first tile starts transparent");
    near(e.scale, 0.78f, "at 0.78");
    e = motion::domino(165.0f, 0);
    near(e.alpha, 1.0f, "opaque after 165 ms");
    near(e.scale, 1.025f, "overshooting to 1.025");
    e = motion::domino(280.0f, 0);
    near(e.scale, 1.0f, "settled after 280 ms");
    e = motion::domino(67.0f, 2);
    near(e.alpha, 0.0f, "a tile two steps on waits 68 ms");
    near(e.scale, 0.78f, "hidden at 0.78 while it waits");
    e = motion::domino(68.0f + 82.5f, 2);
    near(e.alpha, 0.875f, "then rises on its own clock");
    e = motion::domino(68.0f + 165.0f, 2);
    near(e.scale, 1.025f, "and overshoots 165 ms after its delay");
    near(motion::dominoDurationMs(5), 450.0f, "5 steps x 34 + 165 + 115");
}

void pulse() {
    near(motion::pulseScale(0.0f), 1.0f, "the pulse starts at 1");
    near(motion::pulseScale(49.4f), 0.98f, "smoothstep half way down");
    near(motion::pulseScale(98.8f), 0.96f, "dips to 0.96 at 38%");
    near(motion::pulseScale(187.2f), 1.025f, "rises to 1.025 at 72%");
    near(motion::pulseScale(260.0f), 1.0f, "back to 1 after 260 ms");
}

void ring() {
    near(motion::ringAngleDegrees(0.0), 0.0f, "ring starts at 0");
    near(motion::ringAngleDegrees(1400.0), 90.0f, "a quarter turn in 1400 ms");
    near(motion::ringAngleDegrees(2800.0), 180.0f, "half a turn in 2800 ms");
    near(motion::ringAngleDegrees(5600.0), 0.0f, "a full turn in 5600 ms");
}

void scroll() {
    motion::ScrollEaser easer;
    easer.snap(0.0f);
    easer.retarget(100.0f);
    // Unsettled: tau 50 ms, so a 16 ms frame moves 1 - e^(-16/50) of the way.
    expect(easer.step(16.0f, 233.0f, 1256.0f, iideck::ui::ScrollMode::Flow), "it moves");
    near(easer.offset(), 27.385f, "one frame of the 50 ms time constant", 1e-2);
    float previous = easer.offset();
    for (int frame = 0; frame < 120; ++frame) {
        easer.step(16.0f, 233.0f, 1256.0f, iideck::ui::ScrollMode::Flow);
        expect(easer.offset() >= previous && easer.offset() <= 100.0f,
               "approaches without overshoot");
        previous = easer.offset();
    }
    near(easer.offset(), 100.0f, "snaps to the target under half a pixel");
    expect(!easer.step(16.0f, 233.0f, 1256.0f, iideck::ui::ScrollMode::Flow), "settled");

    easer.retarget(2000.0f);
    easer.step(16.0f, 233.0f, 1256.0f, iideck::ui::ScrollMode::Flow);
    // A long jump is capped at min(distance, max(0.22 viewport, 0.85 pitch)) = 276.32.
    near(easer.offset(), 100.0f + 276.32f, "a long jump moves at most its step limit", 1e-2);
}

void railEntrance() {
    near(motion::railEntranceAlpha(motion::RailEntrance{0.0f, 0}), 0.0f,
         "the focused tile starts clear");
    near(motion::railEntranceAlpha(motion::RailEntrance{65.0f, 0}), 0.5f, "half in at 65 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{130.0f, 0}), 1.0f, "in at 130 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{169.0f, 1}), 0.0f,
         "its neighbours wait until 170 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{220.0f, 1}), 0.5f,
         "and are half in 50 ms later");
    near(motion::railEntranceAlpha(motion::RailEntrance{270.0f, 1}), 1.0f, "in at 270 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{270.0f, 2}), 0.0f,
         "the next ones start at 270 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{370.0f, 2}), 1.0f, "and are in at 370 ms");
    near(motion::railEntranceAlpha(motion::RailEntrance{270.0f, 5}), 0.0f, "as are the rest");
}

void visualIndex() {
    motion::VisualIndexEaser easer;
    easer.snap(2.0f);
    near(easer.value(), 2.0f, "snaps to an index");
    expect(easer.settled(), "settled");
    easer.retarget(3.0f);
    expect(!easer.settled(), "heading for a new index");
    // One 16 ms frame covers 1 - (1 - 16/245)^4 of the way.
    easer.step(16.0f);
    near(easer.value(), 2.0f + (1.0f - std::pow(1.0f - 16.0f / 245.0f, 4.0f)), "one frame", 1e-4);
    // A long frame counts as 20 ms.
    motion::VisualIndexEaser slow;
    slow.snap(0.0f);
    slow.retarget(1.0f);
    slow.step(500.0f);
    near(slow.value(), 1.0f - std::pow(1.0f - 20.0f / 245.0f, 4.0f), "a frame is capped at 20 ms",
         1e-4);
    float previous = easer.value();
    for (int frame = 0; frame < 200; ++frame) {
        easer.step(16.0f);
        expect(easer.value() >= previous && easer.value() <= 3.0f, "approaches without overshoot");
        previous = easer.value();
    }
    expect(easer.settled() && easer.value() == 3.0f, "lands exactly on the target");
}

} // namespace

int main() {
    focus();
    domino();
    pulse();
    ring();
    scroll();
    railEntrance();
    visualIndex();
    std::printf("tile_motion: all checks passed\n");
    return 0;
}
