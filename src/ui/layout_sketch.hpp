// layout_sketch — a Library layout drawn small on its card in the START panel: the grid of tiles,
// the XMB's column or the Carousel's row, each tile bordered in one of iiSU's console colours
// (navigation.md §5.2, `roms_layout_chooser_*_light.png`).
#pragma once

#include "gpu2d/ui/widget.hpp"

#include "library/sections.hpp"

namespace opensu::ui {

class LayoutSketch final : public gpu2d::ui::Widget {
  public:
    explicit LayoutSketch(library::LibraryMode mode) noexcept : mode_{mode} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    library::LibraryMode mode_;
};

} // namespace opensu::ui
