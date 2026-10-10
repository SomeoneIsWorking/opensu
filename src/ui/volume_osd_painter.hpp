// volume_osd_painter — draws the volume display: a solid pill with a speaker, the level's bar and
// its percentage, centred below the top bar.
#pragma once

#include "home_layout.hpp"
#include "panel_fade.hpp"
#include "raylib.h"
#include "typeface.hpp"
#include "volume_osd.hpp"

namespace opensu::ui {

class VolumeOsdPainter {
  public:
    explicit VolumeOsdPainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Draws the pill in a frame of `size` at `dp` pixels per dp, `topInset` below the frame's top.
    void paint(const VolumeLevel& level, Vector2 size, float dp, float topInset,
               const PanelLook& look) const;

  private:
    Typeface& typeface_;
};

/// Where the pill stands at rest, in pixels.
[[nodiscard]] Rect volumeOsdBody(Vector2 size, float dp, float topInset) noexcept;

} // namespace opensu::ui
