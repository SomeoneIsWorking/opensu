#include "details_page_painter.hpp"

#include <algorithm>
#include <string>

#include "hud.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color ink{0x2B, 0x27, 0x33, 255};
constexpr Color inkSoft{0x6F, 0x68, 0x80, 255};
constexpr Color cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Color badgeFill{0x2B, 0x27, 0x33, 22};
// iiSU xe4.h's blue, the focus ring's.
constexpr Color focusInk{0x2C, 0x88, 0xFF, 255};
constexpr float titleSp = 30.0f;
constexpr float badgeSp = 13.0f;
constexpr float rowLabelSp = 14.0f;
constexpr float rowValueSp = 16.0f;
constexpr float buttonSp = 17.0f;
constexpr float rowHeightDp = 27.0f;
constexpr float labelColumnDp = 120.0f;
constexpr float outlineDp = 2.5f;
constexpr float backdropAlpha = 0.14f;

/// `text` cut to fit `room` pixels, ending in dots when it is cut.
std::string fitted(std::string text, float room, const TextStyle& style) {
    if (type().measure(text, style) <= room) {
        return text;
    }
    while (!text.empty() && type().measure(text + "...", style) > room) {
        do {
            text.pop_back();
        } while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0U) == 0x80U);
    }
    return text + "...";
}

/// `texture` scaled to fill `box` with its middle showing, or to sit whole inside it when it is
/// much wider than tall.
void drawCover(const Texture& texture, const Rect& box, float radius, float alpha) {
    const auto width = static_cast<float>(texture.width);
    const auto height = static_cast<float>(texture.height);
    const Color tint = withAlpha(WHITE, alpha);
    if (width > height * 1.2f) {
        const float scale = box.width / width;
        const Rect inside{box.x, box.centreY() - height * scale * 0.5f, box.width, height * scale};
        drawTextureRound(RoundRect{inside, radius}, texture, Rectangle{0, 0, width, height}, tint);
        return;
    }
    const float boxAspect = box.width / box.height;
    const float cropWidth = std::min(width, height * boxAspect);
    const float cropHeight = cropWidth / boxAspect;
    drawTextureRound(
        RoundRect{box, radius}, texture,
        Rectangle{(width - cropWidth) * 0.5f, (height - cropHeight) * 0.5f, cropWidth, cropHeight},
        tint);
}

} // namespace

void DetailsPagePainter::paint(const DetailsPage& page, const DetailsLayout& layout,
                               const DetailsArt& art, Vector2 size, float dp, float alpha) {
    if (alpha <= 0.0f) {
        return;
    }
    DrawRectangle(0, 0, static_cast<int>(size.x), static_cast<int>(size.y),
                  withAlpha(palette::ground, alpha));
    if (art.hero != nullptr) {
        const Texture& hero = *art.hero;
        const float scale = std::max(size.x / static_cast<float>(hero.width),
                                     size.y / static_cast<float>(hero.height));
        const float drawn = static_cast<float>(hero.width) * scale;
        const float tall = static_cast<float>(hero.height) * scale;
        DrawTexturePro(
            hero, Rectangle{0, 0, static_cast<float>(hero.width), static_cast<float>(hero.height)},
            Rectangle{(size.x - drawn) * 0.5f, (size.y - tall) * 0.5f, drawn, tall}, Vector2{0, 0},
            0.0f, withAlpha(WHITE, backdropAlpha * alpha));
    }
    const DetailsView& view = page.view();
    paintCover(layout, art, view, alpha);
    paintText(layout, view, dp, alpha);
    paintButtons(page, layout, dp, alpha);
}

void DetailsPagePainter::paintCover(const DetailsLayout& layout, const DetailsArt& art,
                                    const DetailsView& view, float alpha) {
    const RoundRect frame{layout.art, layout.radius};
    fillSoft(frame.grown(2.0f), 10.0f, [alpha](Vector2, float) {
        return withAlpha(Color{0, 0, 0, 60}, alpha);
    });
    if (art.cover != nullptr) {
        drawCover(*art.cover, layout.art, layout.radius, alpha);
        return;
    }
    fillRoundRect(frame, [alpha](Vector2, float) {
        return withAlpha(Color{0xDD, 0xE1, 0xEA, 255}, alpha);
    });
    const TextStyle letter{layout.art.width * 0.4f};
    const std::string initial = view.title.empty() ? "?" : view.title.substr(0, 1);
    const float width = type().measure(initial, letter);
    type().drawCentred(initial, layout.art.centreX() - width * 0.5f, layout.art.centreY(), letter,
                       withAlpha(inkSoft, alpha));
}

void DetailsPagePainter::paintText(const DetailsLayout& layout, const DetailsView& view, float dp,
                                   float alpha) {
    const Rect& text = layout.text;
    const TextStyle title{titleSp * dp};
    float y = text.y + type().lineBox(title) * 0.5f;
    type().drawCentred(fitted(view.title, text.width, title), text.x, y, title,
                       withAlpha(ink, alpha));
    y += type().lineBox(title) * 0.5f + 10.0f * dp;

    const TextStyle badge{badgeSp * dp};
    const float badgeWidth = type().measure(view.badge, badge) + 20.0f * dp;
    const float badgeHeight = type().lineBox(badge) + 8.0f * dp;
    fillRoundRect(RoundRect{Rect{text.x, y, badgeWidth, badgeHeight}, badgeHeight * 0.5f},
                  [alpha](Vector2, float) {
                      return withAlpha(badgeFill, alpha);
                  });
    type().drawCentred(view.badge, text.x + 10.0f * dp, y + badgeHeight * 0.5f, badge,
                       withAlpha(ink, alpha));
    y += badgeHeight + 18.0f * dp;

    const TextStyle label{rowLabelSp * dp};
    const TextStyle value{rowValueSp * dp};
    const float valueX = text.x + labelColumnDp * dp;
    for (const DetailsRow& row : view.rows) {
        if (y + rowHeightDp * dp > text.bottom()) {
            break;
        }
        const float centre = y + rowHeightDp * dp * 0.5f;
        type().drawCentred(row.label, text.x, centre, label, withAlpha(inkSoft, alpha));
        type().drawCentred(fitted(row.value, text.right() - valueX, value), valueX, centre, value,
                           withAlpha(ink, alpha));
        y += rowHeightDp * dp;
    }
}

void DetailsPagePainter::paintButtons(const DetailsPage& page, const DetailsLayout& layout,
                                      float dp, float alpha) {
    const TextStyle text{buttonSp * dp};
    const auto& buttons = page.view().buttons;
    for (std::size_t i = 0; i < buttons.size() && i < layout.buttons.size(); ++i) {
        const Rect& rect = layout.buttons[i];
        const RoundRect body{rect, rect.height * 0.35f};
        const bool focused = i == page.focus();
        fillRoundRect(body, [alpha](Vector2, float) {
            return withAlpha(cardFill, alpha * 0.92f);
        });
        if (focused) {
            fillBand(body, body.grown(-outlineDp * dp), [alpha](Vector2, float) {
                return withAlpha(focusInk, alpha);
            });
        }
        const float pad = 16.0f * dp;
        type().drawCentred(buttons[i].label, rect.x + pad, rect.centreY(), text,
                           withAlpha(ink, alpha));
        if (!buttons[i].value.empty()) {
            const std::string shown = "<  " + buttons[i].value + "  >";
            const float width = type().measure(shown, text);
            type().drawCentred(fitted(shown, rect.width - 2.0f * pad - 90.0f * dp, text),
                               rect.right() - pad - width, rect.centreY(), text,
                               withAlpha(inkSoft, alpha));
        }
    }
}

} // namespace opensu::ui
