#include "icon_widgets.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

/// An icon's side against the value text's size, and the gap beside it.
constexpr float iconShare = 1.35f;
constexpr float gapShare = 0.5f;

float iconSide(const tk::Theme& theme) noexcept {
    return theme.px(theme.type.value) * iconShare;
}

/// Draws `icon` as a mask tinted `ink` in the `side` square centred on `centre`.
void drawIcon(const tk::Painter& painter, ScreenImages& images, Icon icon, gpu2d::Point centre,
              float side, gpu2d::Colour ink) {
    const gpu2d::Texture* mask = images.icon(icon, static_cast<int>(std::lround(side)));
    if (mask == nullptr) {
        return;
    }
    painter.canvas.image(mask->view(),
                         gpu2d::Rect{0.0f, 0.0f, static_cast<float>(mask->width()),
                                     static_cast<float>(mask->height())},
                         gpu2d::Rect{centre.x - side * 0.5f, centre.y - side * 0.5f, side, side},
                         painter.fade(ink));
}

} // namespace

float IconItem::trailingWidth(tk::LayoutContext& context, float width) {
    const float side = iconSide(context.theme);
    const float gap = side * gapShare;
    const float rest = Item::trailingWidth(context, std::max(0.0f, width - side - gap));
    return rest + side + (rest > 0.0f ? gap : 0.0f);
}

void IconItem::paintTrailing(tk::Painter& painter, const gpu2d::Rect& area) const {
    const float side = iconSide(painter.theme);
    drawIcon(painter, images_, icon_,
             gpu2d::Point{area.right() - side * 0.5f, area.y + area.height * 0.5f}, side,
             painter.theme.colours.ink);
    const float rest = area.width - side - side * gapShare;
    if (rest > 0.0f) {
        Item::paintTrailing(painter, gpu2d::Rect{area.x, area.y, rest, area.height});
    }
}

tk::Size IconButton::measure(tk::LayoutContext& context, float maxWidth) {
    const tk::Size base = Button::measure(context, maxWidth);
    if (fixedWidth()) {
        return base;
    }
    // Both sides, so the centred label clears the icon.
    const float side = iconSide(context.theme);
    return tk::Size{std::min(maxWidth, base.width + 2.0f * side * (1.0f + gapShare)), base.height};
}

void IconButton::paint(tk::Painter& painter) const {
    Button::paint(painter);
    const tk::Theme& theme = painter.theme;
    const float side = iconSide(theme);
    const gpu2d::Rect& box = frame();
    const gpu2d::Colour ink =
        kind_ == tk::ButtonKind::Normal ? theme.colours.ink : theme.colours.onAccent;
    drawIcon(painter, images_, icon_,
             gpu2d::Point{box.x + painter.px(theme.spacing.pad) + side * 0.5f,
                          box.y + box.height * 0.5f},
             side, ink);
}

tk::Size IconTab::measure(tk::LayoutContext& context, float maxWidth) {
    const float side = Button::measure(context, maxWidth).height;
    return tk::Size{std::min(maxWidth, side), side};
}

void IconTab::paint(tk::Painter& painter) const {
    const gpu2d::Colour ink = paintFace(painter);
    const gpu2d::Rect& box = frame();
    drawIcon(painter, images_, icon_,
             gpu2d::Point{box.x + box.width * 0.5f, box.y + box.height * 0.5f}, box.height * 0.55f,
             ink);
}

} // namespace opensu::ui
