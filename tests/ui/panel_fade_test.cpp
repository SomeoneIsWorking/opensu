// The panel fade: alpha and scale over time, reversal mid-run, and when the panel is drawn.
#include "panel_fade.hpp"

#include <chrono>
#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;
using opensu::test::near;
using std::chrono::milliseconds;

void showFadesAndGrows() {
    const PanelFade::Clock::time_point t0{};
    PanelFade fade{FadeSpec{120.0f, 120.0f, 0.96f, 140.0f, 1.0f}};
    expect(!fade.visible(t0) && fade.alpha(t0) == 0.0f, "starts hidden");
    fade.show(t0);
    near(fade.alpha(t0 + milliseconds{60}), 0.5, "half way in 60 ms");
    near(fade.alpha(t0 + milliseconds{500}), 1.0, "fully in");
    near(fade.scale(t0), 0.96, "starts at the from scale");
    near(fade.scale(t0 + milliseconds{140}), 1.0, "grown after the scale time");
}

void hideFadesOutThenStopsDrawing() {
    const PanelFade::Clock::time_point t0{};
    PanelFade fade{FadeSpec{110.0f, 100.0f, 0.96f, 110.0f, 0.985f}};
    fade.show(t0);
    const auto shown = t0 + milliseconds{1000};
    fade.hide(shown);
    near(fade.alpha(shown + milliseconds{50}), 0.5, "half way out");
    near(fade.scale(shown + milliseconds{100}), 0.985, "shrinks to the leaving scale");
    expect(fade.visible(shown + milliseconds{50}), "still drawn while fading");
    expect(!fade.visible(shown + milliseconds{100}), "not drawn once gone");
}

void reversalStartsFromWhereItWas() {
    const PanelFade::Clock::time_point t0{};
    PanelFade fade{FadeSpec{100.0f, 100.0f, 0.96f, 100.0f, 1.0f}};
    fade.show(t0);
    fade.hide(t0 + milliseconds{50});
    near(fade.alpha(t0 + milliseconds{50}), 0.5, "hiding picks up the current alpha");
    fade.show(t0 + milliseconds{100});
    expect(fade.alpha(t0 + milliseconds{100}) > 0.0f && fade.alpha(t0 + milliseconds{100}) < 0.5f,
           "showing again does not jump");
}

} // namespace

int main() {
    showFadesAndGrows();
    hideFadesOutThenStopsDrawing();
    reversalStartsFromWhereItWas();
    std::printf("panel_fade: all checks passed\n");
    return 0;
}
