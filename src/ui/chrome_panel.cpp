#include "chrome_panel.hpp"

namespace opensu::ui {

void ChromePanel::arrange(gpu2d::ui::LayoutContext& context, const gpu2d::Rect& frame) {
    frame_ = solid_ ? area_ : frame;
    for (std::size_t i = 0; i < children_.size(); ++i) {
        children_[i]->arrange(context, frames_[i]);
    }
}

void ChromePanel::paint(gpu2d::ui::Painter& painter) const {
    if (draw_) {
        draw_(painter);
    }
}

} // namespace opensu::ui
