// volume_osd_painter — draws the volume display: a solid pill with a speaker, the level's bar and
// its percentage, centred below the top bar.
#pragma once

#include "home_layout.hpp"
#include "panel_fade.hpp"
#include "raylib.h"
#include "volume_osd.hpp"

namespace opensu::ui {

/// Draws the pill in a frame of `size` at `dp` pixels per dp, `topInset` below the frame's top.
void paintVolumeOsd(const VolumeLevel& level, Vector2 size, float dp, float topInset,
                    const PanelLook& look);

/// Where the pill stands at rest, in pixels.
[[nodiscard]] Rect volumeOsdBody(Vector2 size, float dp, float topInset) noexcept;

} // namespace opensu::ui
