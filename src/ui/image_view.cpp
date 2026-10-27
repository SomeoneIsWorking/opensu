#include "image_view.hpp"

#include <algorithm>

#include "round_shape.hpp"
#include "tile_painter.hpp"

namespace opensu::ui {

gpu2d::ui::Size ImageView::measure(gpu2d::ui::LayoutContext& context, float maxWidth) {
    if (const std::optional<float> height = fixedHeight()) {
        const float tall = context.theme.px(*height);
        return gpu2d::ui::Size{tall * aspect_, tall};
    }
    const float wide = fixedWidth() ? context.theme.px(*fixedWidth()) : maxWidth;
    return gpu2d::ui::Size{wide, wide / aspect_};
}

gpu2d::Rect ImageView::box(float width, float height) const noexcept {
    if (!whole_ || width <= 0.0f || height <= 0.0f) {
        return frame_;
    }
    const float scale = std::min(frame_.width / width, frame_.height / height);
    const float drawn = width * scale;
    float x = frame_.x;
    if (*whole_ == gpu2d::ui::Align::Centre) {
        x += (frame_.width - drawn) * 0.5f;
    } else if (*whole_ == gpu2d::ui::Align::End) {
        x += frame_.width - drawn;
    }
    return gpu2d::Rect{x, frame_.y, drawn, height * scale};
}

void ImageView::paint(gpu2d::ui::Painter& painter) const {
    const gpu2d::ui::Theme& theme = painter.theme;
    const float radius = painter.px(theme.shape.smallRadius);
    if (const gpu2d::Texture* texture = images_.find(file_)) {
        const auto width = static_cast<float>(texture->width());
        const auto height = static_cast<float>(texture->height());
        const gpu2d::Rect shown = box(width, height);
        const Rect target{shown.x, shown.y, shown.width, shown.height};
        drawTextureRound(painter.canvas, RoundRect{target, radius}, texture->view(),
                         coverSource(width, height, target),
                         painter.fade(Colour{255, 255, 255, 255}));
        return;
    }
    painter.canvas.fillRoundRect(box(aspect_, 1.0f), radius, painter.fade(theme.colours.control));
}

} // namespace opensu::ui
