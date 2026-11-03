// progress_spinner — Material 3's indeterminate CircularProgressIndicator, which iiSU draws
// around the status pill's bell while tasks run (a32.n, of6.a) and openSU around a starting
// launcher's badge, and in a cover's place while it is on its way.
#pragma once

#include "paint.hpp"

namespace opensu::ui {

/// The arc the spinner draws at one moment, in degrees clockwise from 3 o'clock.
struct SpinnerArc {
    float start{0.0f};
    float sweep{0.0f};
};

/// The arc `seconds` into the animation.
[[nodiscard]] SpinnerArc spinnerArc(double seconds) noexcept;

/// Draws the spinner in a `size` box centred on `centre`, with a `stroke` wide line.
void drawSpinner(Canvas& canvas, Point centre, float size, float stroke, Colour colour,
                 double seconds);

/// The spinner of a cover on its way, centred in `box` at `alpha`: a tile's, the details page's.
void drawCoverLoading(Canvas& canvas, const Rect& box, float alpha, double seconds);

} // namespace opensu::ui
