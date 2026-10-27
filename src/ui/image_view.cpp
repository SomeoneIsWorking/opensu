#include "image_view.hpp"

#include "round_shape.hpp"
#include "tile_painter.hpp"

namespace opensu::ui {

gpu2d::ui::Size ImageView::measure(gpu2d::ui::LayoutContext& context, float maxWidth) {
    images_.want(file_);
    if (const std::optional<float> height = fixedHeight()) {
        const float tall = context.theme.px(*height);
        return gpu2d::ui::Size{tall * aspect_, tall};
    }
    const float wide = fixedWidth() ? context.theme.px(*fixedWidth()) : maxWidth;
    return gpu2d::ui::Size{wide, wide / aspect_};
}

void ImageView::paint(gpu2d::ui::Painter& painter) const {
    const gpu2d::ui::Theme& theme = painter.theme;
    const float radius = painter.px(theme.shape.smallRadius);
    const Rect box{frame_.x, frame_.y, frame_.width, frame_.height};
    if (const gpu2d::Texture* texture = images_.find(file_)) {
        drawTextureRound(painter.canvas, RoundRect{box, radius}, texture->view(),
                         coverSource(static_cast<float>(texture->width()),
                                     static_cast<float>(texture->height()), box),
                         painter.fade(Colour{255, 255, 255, 255}));
        return;
    }
    painter.canvas.fillRoundRect(frame_, radius, painter.fade(theme.colours.control));
}

} // namespace opensu::ui
