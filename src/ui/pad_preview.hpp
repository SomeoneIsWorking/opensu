// pad_preview — a pad's sticks and triggers drawn where they stand: each stick a circle with its
// dot as the pad plays it (after its calibration) over a faint dot where the pad reads it, and
// each trigger a bar filled as far as it plays, a faint mark where it reads.
#pragma once

#include "gpu2d/ui/widget.hpp"

#include "gamepad/pad_translator.hpp"

namespace opensu::ui {

class PadPreview final : public gpu2d::ui::Widget {
  public:
    explicit PadPreview(gamepad::PadAxes axes) : axes_{axes} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    gamepad::PadAxes axes_;
};

} // namespace opensu::ui
