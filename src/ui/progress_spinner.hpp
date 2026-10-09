// progress_spinner — Material 3's indeterminate CircularProgressIndicator, which iiSU draws
// around the status pill's bell while tasks run (a32.n, of6.a).
#pragma once

#include "raylib.h"

namespace opensu::ui {

/// The arc the spinner draws at one moment, in degrees clockwise from 3 o'clock.
struct SpinnerArc {
    float start{0.0f};
    float sweep{0.0f};
};

/// The arc `seconds` into the animation.
[[nodiscard]] SpinnerArc spinnerArc(double seconds) noexcept;

/// Draws the spinner in a `size` box centred on `centre`, with a `stroke` wide line.
void drawSpinner(Vector2 centre, float size, float stroke, Color colour, double seconds);

} // namespace opensu::ui
