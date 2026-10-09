// row_controls — the controls at the end of an option row, shared by the options panel and the
// settings page: a switch, a level slider and a chevron.
#pragma once

#include "raylib.h"

#include "home_layout.hpp"

namespace opensu::ui {

/// The ink of the focus outline, the switch's "on" and the slider's fill.
inline constexpr Color rowInk{0x4D, 0x46, 0x55, 255};
/// The switch's "off" and the slider's track.
inline constexpr Color rowOff{0xB7, 0xB2, 0xC0, 255};

/// A switch in `track`.
void paintSwitch(const Rect& track, bool on, float dp);

/// A slider in `track` from `low` to `high`, filled to `level`, with the number after it.
void paintSlider(const Rect& track, int level, int low, int high, float dp);

/// A right-pointing chevron whose tip is at `x`.
void paintChevron(float x, float centreY, float dp);

/// The room a chevron takes, in dp.
inline constexpr float chevronWidthDp = 14.0f;

} // namespace opensu::ui
