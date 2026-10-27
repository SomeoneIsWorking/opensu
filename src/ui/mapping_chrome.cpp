#include "mapping_chrome.hpp"

namespace opensu::ui {

std::unique_ptr<gpu2d::ui::Widget> MappingChrome::build(const gpu2d::ui::Theme& /*theme*/) {
    auto root = std::make_unique<ChromePanel>([this](gpu2d::ui::Painter& painter) {
        painter_.paint(painter.canvas, page_, Point{frame_.width, frame_.height}, frame_.dp,
                       fade_.look(frame_.now));
    });
    if (page_.isOpen()) {
        root->solid(gpu2d::Rect{0.0f, 0.0f, frame_.width, frame_.height});
    }
    return root;
}

} // namespace opensu::ui
