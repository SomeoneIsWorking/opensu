#include "offer_card.hpp"

#include "gpu2d/ui/text_fit.hpp"

#include "round_shape.hpp"
#include "tile_painter.hpp"

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

/// The room between the card's edge and what is in it, in dp.
float insetDp(const tk::Theme& theme) noexcept {
    return theme.spacing.gap * 2.0f;
}

/// The art's box inside a card at `frame`.
gpu2d::Rect artBox(const tk::Theme& theme, const gpu2d::Rect& frame) noexcept {
    const float inset = theme.px(insetDp(theme));
    const float width = frame.width - 2.0f * inset;
    return gpu2d::Rect{frame.x + inset, frame.y + inset, width, width / OfferCard::artAspect};
}

/// The badge's width at the end of the line, gaps included; 0 without one.
float badgeWidth(tk::Typesetter& text, const tk::Theme& theme, const std::string& badge) {
    if (badge.empty()) {
        return 0.0f;
    }
    return text.measure(badge, theme.px(theme.type.caption)) + theme.px(theme.spacing.gap * 3.0f);
}

} // namespace

tk::Size OfferCard::measure(tk::LayoutContext& context, float maxWidth) {
    const tk::Theme& theme = context.theme;
    const float width = fixedWidth() ? theme.px(*fixedWidth()) : maxWidth;
    const float inset = theme.px(insetDp(theme));
    const float art = (width - 2.0f * inset) / artAspect;
    const float title = context.text.lineHeight(theme.px(theme.type.body));
    const float line = context.text.lineHeight(theme.px(theme.type.caption));
    return tk::Size{width, inset + art + inset * 0.75f + title + theme.px(4.0f) + line + inset};
}

void OfferCard::arrange(tk::LayoutContext& context, const gpu2d::Rect& frame) {
    frame_ = frame;
    images_.want(art_);
    const tk::Theme& theme = context.theme;
    const float room = frame.width - 2.0f * theme.px(insetDp(theme));
    titleShown_ = tk::fitted(context.text, text_.title, room, theme.px(theme.type.body));
    lineShown_ =
        tk::fitted(context.text, text_.line, room - badgeWidth(context.text, theme, text_.badge),
                   theme.px(theme.type.caption));
}

float OfferCard::cornerRadius(const tk::Theme& theme) const {
    return theme.px(theme.shape.radius);
}

bool OfferCard::act(tk::Action action, tk::ActContext& context) {
    if (action != tk::Action::Accept || !pressed_) {
        return false;
    }
    context.sound(tk::Sound::Activate);
    pressed_();
    return true;
}

std::vector<tk::Hint> OfferCard::hints() const {
    return {tk::Hint{{tk::Action::Accept}, "Details"}};
}

void OfferCard::paint(tk::Painter& painter) const {
    const tk::Theme& theme = painter.theme;
    const bool focused = painter.focused == id();
    painter.canvas.fillRoundRect(
        frame_, cornerRadius(theme),
        painter.fade(focused ? theme.colours.controlFocused : theme.colours.control));
    const gpu2d::Rect art = artBox(theme, frame_);
    const Rect box{art.x, art.y, art.width, art.height};
    const float radius = painter.px(theme.shape.smallRadius);
    if (const gpu2d::Texture* texture = images_.find(art_)) {
        drawTextureRound(painter.canvas, RoundRect{box, radius}, texture->view(),
                         coverSource(static_cast<float>(texture->width()),
                                     static_cast<float>(texture->height()), box),
                         painter.fade(Colour{255, 255, 255, 255}));
    } else {
        painter.canvas.fillRoundRect(art, radius, painter.fade(theme.colours.surface));
    }
    const float inset = painter.px(insetDp(theme));
    const float title = painter.text.lineHeight(painter.px(theme.type.body));
    const float line = painter.text.lineHeight(painter.px(theme.type.caption));
    const float titleY = art.y + art.height + inset * 0.75f + title * 0.5f;
    const float lineY = titleY + title * 0.5f + painter.px(4.0f) + line * 0.5f;
    painter.label(titleShown_, gpu2d::Point{art.x, titleY}, theme.type.body, theme.colours.ink);
    painter.label(lineShown_, gpu2d::Point{art.x, lineY}, theme.type.caption,
                  theme.colours.inkMuted);
    if (text_.badge.empty()) {
        return;
    }
    const float width =
        badgeWidth(painter.text, theme, text_.badge) - painter.px(theme.spacing.gap);
    const gpu2d::Rect pill{art.x + art.width - width, lineY - line * 0.5f - painter.px(2.0f), width,
                           line + painter.px(4.0f)};
    const float round = pill.height * 0.5f;
    if (text_.strongBadge) {
        painter.canvas.fillRoundRect(pill, round, painter.fade(theme.colours.accent));
    } else {
        painter.canvas.strokeRoundRect(pill, round, painter.px(theme.shape.outline),
                                       painter.fade(theme.colours.inkFaint));
    }
    const float words = painter.text.measure(text_.badge, painter.px(theme.type.caption));
    painter.label(text_.badge, gpu2d::Point{pill.x + (pill.width - words) * 0.5f, lineY},
                  theme.type.caption,
                  text_.strongBadge ? theme.colours.onAccent : theme.colours.inkMuted);
}

} // namespace opensu::ui
