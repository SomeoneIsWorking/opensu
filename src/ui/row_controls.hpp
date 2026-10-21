// row_controls — the controls at the end of an option row, shared by the options panel and the
// settings page: a switch, a checkbox, a level slider and a chevron.
#pragma once

#include "paint.hpp"

#include "home_layout.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// The ink of the focus outline, the switch's "on" and the slider's fill.
inline constexpr Colour rowInk{0x4D, 0x46, 0x55, 255};
/// A chosen category, the lit part of a focused row, a tab not on show.
inline constexpr Colour rowChosen{0xE6, 0xE4, 0xEE, 255};
/// The switch's "off" and the slider's track.
inline constexpr Colour rowOff{0xB7, 0xB2, 0xC0, 255};

/// A checkbox in `box`: an outline, filled and ticked when `checked`.
void paintCheckbox(Canvas& canvas, const Rect& box, bool checked, float dp);

/// A switch in `track`.
void paintSwitch(Canvas& canvas, const Rect& track, bool on, float dp);

/// A slider in `track` from `low` to `high`, filled to `level`, with the number after it.
void paintSlider(Canvas& canvas, Typeface& typeface, const Rect& track, int level, int low,
                 int high, float dp);

/// A right-pointing chevron whose tip is at `x`.
void paintChevron(Canvas& canvas, float x, float centreY, float dp);

/// The room a chevron takes, in dp.
inline constexpr float chevronWidthDp = 14.0f;

} // namespace opensu::ui
