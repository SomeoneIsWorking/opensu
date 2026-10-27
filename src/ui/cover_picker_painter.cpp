#include "cover_picker_painter.hpp"

#include <array>

#include "hint_row.hpp"
#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"
#include "tile_geometry.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour cardFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour cellFill{0xE6, 0xE4, 0xEC, 255};
constexpr Colour outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float titleSp = 19.0f;
constexpr float statusSp = 14.0f;
constexpr float labelSp = 14.0f;
constexpr float cellRadiusDp = 12.0f;
constexpr float outlineDp = 2.4f;
constexpr float labelGapDp = 14.0f;

} // namespace

void CoverPickerPainter::paint(Canvas& canvas, const CoverPicker& picker, const CoverPickerArt& art,
                               Point size, float dp, const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, look.alpha));
    const CoverPickerLayout layout = picker.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    const PanelScope scope{canvas, Point{layout.card.centreX(), layout.card.centreY()}, look.scale};
    fillRoundRect(canvas, RoundRect{layout.card, layout.radius}, [&](Point, float) {
        return withAlpha(cardFill, look.alpha);
    });
    const TextStyle title{titleSp * dp};
    typeface_.drawCentred(canvas, typeface_.fitted(picker.title(), layout.title.width, title),
                          layout.title.x, layout.title.centreY(), title,
                          withAlpha(palette::ink, look.alpha));
    const TextStyle status{statusSp * dp};
    typeface_.drawCentred(canvas, typeface_.fitted(picker.status(), layout.status.width, status),
                          layout.status.x, layout.status.centreY(), status,
                          withAlpha(palette::inkSoft, look.alpha));

    const TextStyle label{labelSp * dp};
    for (std::size_t i = 0; i < layout.cells.size(); ++i) {
        const std::size_t index = layout.first + i;
        const Rect& cell = layout.cells[i];
        const RoundRect box{cell, cellRadiusDp * dp};
        fillRoundRect(canvas, box, [&](Point, float) {
            return withAlpha(cellFill, look.alpha);
        });
        if (const gpu2d::Texture* texture = art.find(index)) {
            const auto width = static_cast<float>(texture->width());
            const auto height = static_cast<float>(texture->height());
            // The whole cover, so its own shape shows.
            const Rect fitted = containFit(width, height, cell);
            drawTextureRound(canvas, RoundRect{fitted, cellRadiusDp * dp}, texture->view(),
                             gpu2d::Rect{0.0f, 0.0f, width, height}, withAlpha(white, look.alpha));
        }
        if (index == picker.focus()) {
            fillBand(canvas, box.grown(outlineDp * dp), box, [&](Point, float) {
                return withAlpha(outlineInk, look.alpha);
            });
        }
        const std::string& text = picker.picks()[index].label;
        const float labelWidth = typeface_.measure(text, label);
        typeface_.drawCentred(canvas, text, cell.centreX() - labelWidth * 0.5f,
                              cell.bottom() + labelGapDp * dp, label,
                              withAlpha(palette::ink, look.alpha));
    }

    constexpr std::array<HintPrompt, 2> plain{{{"A", "Choose"}, {"B", "Close"}}};
    constexpr std::array<HintPrompt, 3> connecting{
        {{"A", "Choose"}, {"Y", "Connect SteamGridDB"}, {"B", "Close"}}};
    const std::span<const HintPrompt> hints =
        picker.offersConnect() ? std::span<const HintPrompt>{connecting} : plain;
    paintHintRow(canvas, glyphs_, typeface_, hints,
                 Point{layout.card.centreX(), layout.hints.centreY()}, dp,
                 withAlpha(outlineInk, look.alpha));
}

} // namespace opensu::ui
