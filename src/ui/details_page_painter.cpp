#include "details_page_painter.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "hud.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Colour ink{0x2B, 0x27, 0x33, 255};
constexpr Colour inkSoft{0x6F, 0x68, 0x80, 255};
constexpr Colour cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour badgeFill{0x2B, 0x27, 0x33, 22};
// iiSU xe4.h's blue, the focus ring's.
constexpr Colour focusInk{0x2C, 0x88, 0xFF, 255};
constexpr float titleSp = 30.0f;
constexpr float badgeSp = 13.0f;
constexpr float rowLabelSp = 14.0f;
constexpr float rowValueSp = 16.0f;
constexpr float buttonSp = 17.0f;
/// Between the title and the badge, around the badge's text, and below the badge.
constexpr float titleGapDp = 10.0f;
constexpr float badgePadDp = 8.0f;
constexpr float badgeGapDp = 18.0f;
constexpr float rowHeightDp = 27.0f;
constexpr float labelColumnDp = 120.0f;
/// Rows that do not fit above the buttons go beside them when that column is this wide.
constexpr float sideMinimumDp = 120.0f;
constexpr float sideGapDp = 8.0f;
/// The least room between a button's label and its value.
constexpr float valueGapDp = 12.0f;
constexpr float outlineDp = 2.5f;
constexpr float backdropAlpha = 0.14f;

/// `texture` scaled to fill `box` with its middle showing, or to sit whole inside it when it is
/// much wider than tall.
void drawCover(Canvas& canvas, const gpu2d::Texture& texture, const Rect& box, float radius,
               float alpha) {
    const auto width = static_cast<float>(texture.width());
    const auto height = static_cast<float>(texture.height());
    const Colour tint = withAlpha(white, alpha);
    if (width > height * 1.2f) {
        const float scale = box.width / width;
        const Rect inside{box.x, box.centreY() - height * scale * 0.5f, box.width, height * scale};
        drawTextureRound(canvas, RoundRect{inside, radius}, texture.view(),
                         gpu2d::Rect{0, 0, width, height}, tint);
        return;
    }
    const float boxAspect = box.width / box.height;
    const float cropWidth = std::min(width, height * boxAspect);
    const float cropHeight = cropWidth / boxAspect;
    drawTextureRound(canvas, RoundRect{box, radius}, texture.view(),
                     gpu2d::Rect{(width - cropWidth) * 0.5f, (height - cropHeight) * 0.5f,
                                 cropWidth, cropHeight},
                     tint);
}

} // namespace

void DetailsPagePainter::paint(Canvas& canvas, const DetailsPage& page, const DetailsLayout& layout,
                               const DetailsArt& art, Point size, float dp, float alpha) const {
    if (alpha <= 0.0f) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, std::floor(size.x), std::floor(size.y)},
                    withAlpha(palette::ground, alpha));
    if (art.hero != nullptr) {
        const gpu2d::Texture& hero = *art.hero;
        const float scale = std::max(size.x / static_cast<float>(hero.width()),
                                     size.y / static_cast<float>(hero.height()));
        const float drawn = static_cast<float>(hero.width()) * scale;
        const float tall = static_cast<float>(hero.height()) * scale;
        canvas.image(hero.view(), wholeOf(hero),
                     gpu2d::Rect{(size.x - drawn) * 0.5f, (size.y - tall) * 0.5f, drawn, tall},
                     withAlpha(white, backdropAlpha * alpha));
    }
    const DetailsView& view = page.view();
    paintCover(canvas, layout, art, view, alpha);
    paintText(canvas, layout, page, dp, alpha);
    paintButtons(canvas, page, layout, dp, alpha);
}

void DetailsPagePainter::paintCover(Canvas& canvas, const DetailsLayout& layout,
                                    const DetailsArt& art, const DetailsView& view,
                                    float alpha) const {
    const RoundRect frame{layout.art, layout.radius};
    fillSoft(canvas, frame.grown(2.0f), 10.0f, [alpha](Point, float) {
        return withAlpha(Colour{0, 0, 0, 60}, alpha);
    });
    if (art.cover != nullptr) {
        drawCover(canvas, *art.cover, layout.art, layout.radius, alpha);
        return;
    }
    fillRoundRect(canvas, frame, [alpha](Point, float) {
        return withAlpha(Colour{0xDD, 0xE1, 0xEA, 255}, alpha);
    });
    const TextStyle letter{layout.art.width * 0.4f};
    const std::string initial = view.title.empty() ? "?" : view.title.substr(0, 1);
    const float width = typeface_.measure(initial, letter);
    typeface_.drawCentred(canvas, initial, layout.art.centreX() - width * 0.5f,
                          layout.art.centreY(), letter, withAlpha(inkSoft, alpha));
}

float DetailsPagePainter::headerHeight(float dp) const noexcept {
    return typeface_.lineBox(TextStyle{titleSp * dp}) + titleGapDp * dp +
           typeface_.lineBox(TextStyle{badgeSp * dp}) + badgePadDp * dp + badgeGapDp * dp;
}

void DetailsPagePainter::paintText(Canvas& canvas, const DetailsLayout& layout,
                                   const DetailsPage& page, float dp, float alpha) const {
    const DetailsView& view = page.view();
    const Rect& text = layout.text;
    const TextStyle title{titleSp * dp};
    float y = text.y + typeface_.lineBox(title) * 0.5f;
    typeface_.drawCentred(canvas, typeface_.fitted(view.title, text.width, title), text.x, y, title,
                          withAlpha(ink, alpha));
    y += typeface_.lineBox(title) * 0.5f + titleGapDp * dp;

    const TextStyle badge{badgeSp * dp};
    const float badgeWidth = typeface_.measure(view.badge, badge) + 20.0f * dp;
    const float badgeHeight = typeface_.lineBox(badge) + badgePadDp * dp;
    fillRoundRect(canvas, RoundRect{Rect{text.x, y, badgeWidth, badgeHeight}, badgeHeight * 0.5f},
                  [alpha](Point, float) {
                      return withAlpha(badgeFill, alpha);
                  });
    typeface_.drawCentred(canvas, view.badge, text.x + 10.0f * dp, y + badgeHeight * 0.5f, badge,
                          withAlpha(ink, alpha));
    y += badgeHeight + badgeGapDp * dp;

    const TextStyle label{rowLabelSp * dp};
    const TextStyle value{rowValueSp * dp};
    const float valueX = text.x + labelColumnDp * dp;
    const auto rows = static_cast<float>(page.rows().size());
    if (y + rows * rowHeightDp * dp > text.bottom() && layout.side.width >= sideMinimumDp * dp) {
        paintSideRows(canvas, Rect{layout.side.x, y, layout.side.width, layout.side.bottom() - y},
                      page, dp, alpha);
        return;
    }
    for (const DetailsRow& row : page.rows()) {
        if (y + rowHeightDp * dp > text.bottom()) {
            break;
        }
        const float centre = y + rowHeightDp * dp * 0.5f;
        typeface_.drawCentred(canvas, row.label, text.x, centre, label, withAlpha(inkSoft, alpha));
        typeface_.drawCentred(canvas, typeface_.fitted(row.value, text.right() - valueX, value),
                              valueX, centre, value, withAlpha(ink, alpha));
        y += rowHeightDp * dp;
    }
}

void DetailsPagePainter::paintSideRows(Canvas& canvas, const Rect& side, const DetailsPage& page,
                                       float dp, float alpha) const {
    const TextStyle label{rowLabelSp * dp};
    const TextStyle value{rowValueSp * dp};
    const float labelBox = typeface_.lineBox(label);
    const float valueBox = typeface_.lineBox(value);
    float y = side.y;
    for (const DetailsRow& row : page.rows()) {
        if (y + labelBox + valueBox > side.bottom()) {
            break;
        }
        typeface_.drawCentred(canvas, row.label, side.x, y + labelBox * 0.5f, label,
                              withAlpha(inkSoft, alpha));
        y += labelBox;
        for (const std::string& line : typeface_.wrapped(row.value, side.width, value)) {
            if (y + valueBox > side.bottom()) {
                break;
            }
            typeface_.drawCentred(canvas, typeface_.fitted(line, side.width, value), side.x,
                                  y + valueBox * 0.5f, value, withAlpha(ink, alpha));
            y += valueBox;
        }
        y += sideGapDp * dp;
    }
}

void DetailsPagePainter::paintButtons(Canvas& canvas, const DetailsPage& page,
                                      const DetailsLayout& layout, float dp, float alpha) const {
    const TextStyle text{buttonSp * dp};
    const auto& buttons = page.view().buttons;
    for (std::size_t i = 0; i < buttons.size() && i < layout.buttons.size(); ++i) {
        const Rect& rect = layout.buttons[i];
        const RoundRect body{rect, rect.height * 0.35f};
        const bool focused = i == page.focus();
        fillRoundRect(canvas, body, [alpha](Point, float) {
            return withAlpha(cardFill, alpha * 0.92f);
        });
        if (focused) {
            fillBand(canvas, body, body.grown(-outlineDp * dp), [alpha](Point, float) {
                return withAlpha(focusInk, alpha);
            });
        }
        const float pad = 16.0f * dp;
        typeface_.drawCentred(canvas,
                              typeface_.fitted(page.shown(i), rect.width - 2.0f * pad, text),
                              rect.x + pad, rect.centreY(), text, withAlpha(ink, alpha));
        if (!buttons[i].value.empty()) {
            // Only a choice steps with Left and Right; other values are read-outs.
            const bool steps = buttons[i].action == DetailsAction::Emulator ||
                               buttons[i].action == DetailsAction::Setting;
            const std::string shown = steps ? "<  " + buttons[i].value + "  >" : buttons[i].value;
            const float label = typeface_.measure(page.shown(i), text);
            const std::string fitted =
                typeface_.fitted(shown, rect.width - 2.0f * pad - label - valueGapDp * dp, text);
            typeface_.drawCentred(canvas, fitted,
                                  rect.right() - pad - typeface_.measure(fitted, text),
                                  rect.centreY(), text, withAlpha(inkSoft, alpha));
        }
    }
}

} // namespace opensu::ui
