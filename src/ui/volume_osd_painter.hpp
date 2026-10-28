// volume_osd_painter — draws the volume display: a solid pill with a speaker, the level's bar and
// its percentage, centred below the top bar.
#pragma once

#include "gpu2d/ui/typesetter.hpp"
#include "home_layout.hpp"
#include "paint.hpp"
#include "panel_fade.hpp"
#include "volume_osd.hpp"

namespace opensu::ui {

class VolumeOsdPainter {
  public:
    explicit VolumeOsdPainter(gpu2d::ui::Typesetter& text) noexcept : text_{text} {
    }

    /// Draws the pill in a frame of `size` at `dp` pixels per dp, `topInset` below the frame's top.
    void paint(Canvas& canvas, const VolumeLevel& level, Point size, float dp, float topInset,
               const PanelLook& look) const;

  private:
    gpu2d::ui::Typesetter& text_;
};

/// Where the pill stands at rest, in pixels.
[[nodiscard]] Rect volumeOsdBody(Point size, float dp, float topInset) noexcept;

} // namespace opensu::ui
