#include "mapping_page_painter.hpp"

#include <array>
#include <span>
#include <string>

#include "hint_row.hpp"
#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour cardFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour rowFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float titleSp = 19.0f;
constexpr float noteSp = 13.0f;
constexpr float rowSp = 16.0f;
constexpr float rowInsetDp = 14.0f;
constexpr float rowRadiusDp = 12.0f;
constexpr float outlineDp = 1.8f;
constexpr float glyphDp = 22.0f;
constexpr float glyphGapDp = 6.0f;

constexpr std::array<HintPrompt, 2> rowHints{{{"A", "Change"}, {"B", "Close"}}};
constexpr std::array<HintPrompt, 2> pickHints{{{"A", "Choose"}, {"B", "Back"}}};
constexpr std::array<HintPrompt, 1> listenHints{{{"B", "Cancel"}}};

} // namespace

void MappingPagePainter::paint(Canvas& canvas, const MappingPage& page, Point size, float dp,
                               const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, look.alpha));
    const MappingLayout layout = page.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    const PanelScope scope{canvas, Point{layout.card.centreX(), layout.card.centreY()}, look.scale};
    fillRoundRect(canvas, RoundRect{layout.card, layout.radius}, [&](Point, float) {
        return withAlpha(cardFill, look.alpha);
    });

    const bool picking = page.mode() == MappingMode::Picking;
    const TextStyle title{titleSp * dp};
    typeface_.drawCentred(
        canvas,
        typeface_.fitted(picking ? page.pickTitle() : page.title(), layout.title.width, title),
        layout.title.x, layout.title.centreY(), title, withAlpha(palette::ink, look.alpha));
    const TextStyle note{noteSp * dp};
    typeface_.drawCentred(canvas, typeface_.fitted(page.note(), layout.note.width, note),
                          layout.note.x, layout.note.centreY(), note,
                          withAlpha(palette::inkSoft, look.alpha));

    const TextStyle row{rowSp * dp};
    const float inset = rowInsetDp * dp;
    const float glyph = glyphDp * dp;
    for (std::size_t shown = 0; shown < layout.rows.size(); ++shown) {
        const std::size_t index = layout.first + shown;
        const Rect& box = layout.rows[shown];
        const RoundRect bar{box, rowRadiusDp * dp};
        fillRoundRect(canvas, bar, [&](Point, float) {
            return withAlpha(rowFill, look.alpha);
        });
        float x = box.x + inset;
        float right = box.right() - inset;
        if (picking) {
            typeface_.drawCentred(canvas, typeface_.fitted(page.choices()[index], right - x, row),
                                  x, box.centreY(), row, withAlpha(palette::ink, look.alpha));
        } else {
            const MappingRow& line = page.rows()[index];
            for (const std::string& key : line.glyphs) {
                const float advance = glyphs_.advance(key, glyph);
                glyphs_.paint(canvas, key, Point{x + advance * 0.5f, box.centreY()}, glyph,
                              withAlpha(outlineInk, look.alpha));
                x += advance + glyphGapDp * dp;
            }
            const std::string value = typeface_.fitted(line.value, (right - x) * 0.5f, row);
            const float valueWidth = typeface_.measure(value, row);
            typeface_.drawCentred(canvas, value, right - valueWidth, box.centreY(), row,
                                  withAlpha(palette::inkSoft, look.alpha));
            right -= valueWidth + inset;
            typeface_.drawCentred(canvas, typeface_.fitted(line.label, right - x, row), x,
                                  box.centreY(), row, withAlpha(palette::ink, look.alpha));
        }
        if (index == page.line()) {
            fillBand(canvas, bar, bar.grown(-outlineDp * dp), [&](Point, float) {
                return withAlpha(outlineInk, look.alpha);
            });
        }
    }
    paintHints(canvas, page, layout, dp, look.alpha);
}

void MappingPagePainter::paintHints(Canvas& canvas, const MappingPage& page,
                                    const MappingLayout& layout, float dp, float alpha) const {
    const std::span<const HintPrompt> hints = page.mode() == MappingMode::Picking ? pickHints
                                              : page.mode() == MappingMode::Listening
                                                  ? std::span<const HintPrompt>{listenHints}
                                                  : rowHints;
    paintHintRow(canvas, glyphs_, typeface_, hints,
                 Point{layout.card.centreX(), layout.hints.centreY()}, dp,
                 withAlpha(outlineInk, alpha));
}

} // namespace opensu::ui
