#include "mode_chooser_painter.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "clip_stack.hpp"
#include "hud.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Color cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Color cardShadow{0, 0, 0, 22};
constexpr Color outlineInk{0x4D, 0x46, 0x55, 255};
// The sketches' tiles take these borders in turn, as iiSU's console cards do.
constexpr std::array<Color, 6> tileBorders{{{0xE0, 0x31, 0x5A, 255},
                                            {0x3A, 0x87, 0xD8, 255},
                                            {0xC8, 0x5A, 0xB8, 255},
                                            {0x2F, 0xB9, 0xA0, 255},
                                            {0xE8, 0x92, 0x3A, 255},
                                            {0x7A, 0x5A, 0xD8, 255}}};
constexpr Color switchOff{0xB7, 0xB2, 0xC0, 255};
constexpr float rowLabelSp = 17.0f;
constexpr float valueSp = 16.0f;
constexpr float chevronWidthDp = 14.0f;
constexpr float chevronHalfHeightDp = 5.5f;
constexpr float chevronStrokeDp = 2.0f;
constexpr float rowInsetDp = 16.0f;
constexpr float rowRadiusDp = 12.0f;
constexpr float sliderBarDp = 4.0f;
constexpr float sliderThumbDp = 8.0f;
constexpr float sliderNumberGapDp = 14.0f;
constexpr float outlineDp = 1.8f;
constexpr float dotInsetDp = 17.0f;
constexpr float dotOuterDp = 8.0f;
constexpr float dotInnerDp = 3.1f;
constexpr float labelSp = 19.5f;
constexpr float labelAboveBottomDp = 15.5f;
constexpr float cardShadowBlurDp = 3.0f;
constexpr float tileBorderDp = 1.4f;
// A sketched tile's corner, as a share of its side.
constexpr float tileCorner = 0.16f;

// The sketches, as shares of the card's side (measured on `roms_layout_chooser_xmb_light.png`
// at 352 px): the grid's tiles and pitch, the rails' small and focused tiles and their gap.
constexpr float gridTile = 0.256f;
constexpr float gridPitch = 0.29f;
constexpr float gridLeft = 0.082f;
constexpr float gridTop = 0.085f;
constexpr float railSmall = 0.187f;
constexpr float railFocused = 0.364f;
constexpr float railGap = 0.03f;
constexpr float xmbLeft = 0.20f;
constexpr float carouselBottom = 0.79f;

Color whitened(Color colour) noexcept {
    return mix(colour, WHITE, 0.82f);
}

/// One sketched tile: where it stands and which of the border colours it takes.
struct Sketched {
    Rect rect;
    std::size_t colour{};
};

void sketchTile(const Sketched& tile, float dp) {
    const Color border = tileBorders[tile.colour % tileBorders.size()];
    const float radius = std::min(tile.rect.width, tile.rect.height) * tileCorner;
    const RoundRect outer{tile.rect, radius};
    fillRoundRect(outer, [border](Vector2, float) {
        return border;
    });
    fillRoundRect(outer.grown(-tileBorderDp * dp), [border](Vector2, float) {
        return whitened(border);
    });
}

/// A mode's sketch inside `card`, clipped to it.
void sketch(library::LibraryMode mode, const Rect& card, float dp, ClipStack& clips) {
    const float side = card.width;
    const ScopedClip clip{clips, card};
    if (mode == library::LibraryMode::Standard) {
        for (std::size_t column = 0; column < 5; ++column) {
            for (std::size_t row = 0; row < 3; ++row) {
                sketchTile(
                    Sketched{
                        Rect{card.x + (gridLeft + static_cast<float>(column) * gridPitch) * side,
                             card.y + (gridTop + static_cast<float>(row) * gridPitch) * side,
                             gridTile * side, gridTile * side},
                        column * 3 + row},
                    dp);
            }
        }
    } else if (mode == library::LibraryMode::Xmb) {
        const float left = card.x + xmbLeft * side;
        const float small = railSmall * side;
        const float big = railFocused * side;
        const float gap = railGap * side;
        const float middle = card.centreY();
        sketchTile(Sketched{Rect{left, middle - big * 0.5f, big, big}, 3}, dp);
        for (std::size_t step = 1; step <= 3; ++step) {
            const auto away = static_cast<float>(step);
            sketchTile(
                Sketched{Rect{left, middle - big * 0.5f - gap - away * small - (away - 1.0f) * gap,
                              small, small},
                         3 + tileBorders.size() - step},
                dp);
            sketchTile(
                Sketched{Rect{left, middle + big * 0.5f + gap + (away - 1.0f) * (small + gap),
                              small, small},
                         3 + step},
                dp);
        }
    } else {
        const float small = railSmall * side;
        const float big = railFocused * side;
        const float gap = railGap * side;
        const float bottom = card.y + carouselBottom * side;
        const float middle = card.centreX();
        sketchTile(Sketched{Rect{middle - big * 0.5f, bottom - big, big, big}, 2}, dp);
        for (std::size_t step = 1; step <= 3; ++step) {
            const auto away = static_cast<float>(step);
            sketchTile(Sketched{Rect{middle - big * 0.5f - gap - away * small - (away - 1.0f) * gap,
                                     bottom - small, small, small},
                                2 + tileBorders.size() - step},
                       dp);
            sketchTile(Sketched{Rect{middle + big * 0.5f + gap + (away - 1.0f) * (small + gap),
                                     bottom - small, small, small},
                                2 + step},
                       dp);
        }
    }
}

/// A switch at the end of a row.
void paintSwitch(const Rect& track, bool on, float dp) {
    const float radius = track.height * 0.5f;
    fillRoundRect(RoundRect{track, radius}, [on](Vector2, float) {
        return on ? outlineInk : switchOff;
    });
    const float thumb = radius - 3.0f * dp;
    const float thumbX = on ? track.right() - radius : track.x + radius;
    DrawCircleV(Vector2{thumbX, track.centreY()}, thumb, WHITE);
}

/// The icon size slider: a track filled to the level, a thumb on it and the number after it.
void paintSlider(const Rect& track, int level, float dp) {
    const float radius = sliderBarDp * dp * 0.5f;
    const Rect bar{track.x, track.centreY() - radius, track.width, radius * 2.0f};
    fillRoundRect(RoundRect{bar, radius}, [](Vector2, float) {
        return switchOff;
    });
    const float along =
        static_cast<float>(level - minIconLevel) / static_cast<float>(maxIconLevel - minIconLevel);
    const float thumbX = track.x + along * track.width;
    fillRoundRect(RoundRect{Rect{bar.x, bar.y, thumbX - bar.x, bar.height}, radius},
                  [](Vector2, float) {
                      return outlineInk;
                  });
    DrawCircleV(Vector2{thumbX, track.centreY()}, sliderThumbDp * dp, outlineInk);
    DrawCircleV(Vector2{thumbX, track.centreY()}, sliderThumbDp * dp * 0.4f, WHITE);
    const TextStyle number{valueSp * dp};
    type().drawCentred(std::to_string(level), track.right() + sliderNumberGapDp * dp,
                       track.centreY(), number, palette::ink);
}

/// What a row reads and, for a row with a value to cycle, what it stands at.
struct RowText {
    std::string label;
    std::string value;
};

RowText textOf(ChooserRow row, const ChooserValues& values) {
    switch (row) {
    case ChooserRow::IconSize:
        return {"Icon size", ""};
    case ChooserRow::Pin:
        return {"Pin navigation bar on Library", ""};
    case ChooserRow::Sort:
        return {"Sort by", values.sort};
    case ChooserRow::Source:
        return {"Show", values.source};
    case ChooserRow::Installed:
        return {"Installed games only", ""};
    case ChooserRow::Hidden:
        return {"Hidden games only", ""};
    case ChooserRow::Search:
        return {"Search", ""};
    case ChooserRow::Cards:
        break;
    }
    return {};
}

/// A right-pointing chevron whose tip is at `x`.
void paintChevron(float x, float centreY, float dp) {
    const float half = chevronHalfHeightDp * dp;
    const float depth = chevronWidthDp * dp * 0.5f;
    const float thickness = chevronStrokeDp * dp;
    DrawLineEx(Vector2{x - depth, centreY - half}, Vector2{x, centreY}, thickness,
               static_cast<Color>(palette::inkSoft));
    DrawLineEx(Vector2{x, centreY}, Vector2{x - depth, centreY + half}, thickness,
               static_cast<Color>(palette::inkSoft));
}

/// One option row: a white bar with its label, its value or control, and the focus outline.
void paintRow(const ChooserRowBox& box, const ModeChooser& chooser, float dp) {
    const RoundRect bar{box.rect, rowRadiusDp * dp};
    fillRoundRect(bar, [](Vector2, float) {
        return cardFill;
    });
    const ChooserValues& values = chooser.values();
    const RowText text = textOf(box.row, values);
    const TextStyle label{rowLabelSp * dp};
    type().drawCentred(text.label, box.rect.x + rowInsetDp * dp, box.rect.centreY(), label,
                       palette::ink);
    const TextStyle value{valueSp * dp};
    const float endX = box.rect.right() - rowInsetDp * dp;
    switch (box.row) {
    case ChooserRow::IconSize:
        paintSlider(box.control, values.iconSize, dp);
        break;
    case ChooserRow::Pin:
        paintSwitch(box.control, values.pinned, dp);
        break;
    case ChooserRow::Installed:
        paintSwitch(box.control, values.installedOnly, dp);
        break;
    case ChooserRow::Hidden:
        paintSwitch(box.control, values.hiddenOnly, dp);
        break;
    case ChooserRow::Sort:
    case ChooserRow::Source: {
        const float arrowEdge = endX - chevronWidthDp * dp;
        type().drawCentred(text.value, arrowEdge - type().measure(text.value, value),
                           box.rect.centreY(), value, palette::inkSoft);
        paintChevron(endX, box.rect.centreY(), dp);
        break;
    }
    case ChooserRow::Search:
        paintChevron(endX, box.rect.centreY(), dp);
        break;
    case ChooserRow::Cards:
        break;
    }
    if (chooser.row() == box.row) {
        fillBand(bar, bar.grown(-outlineDp * dp), [](Vector2, float) {
            return outlineInk;
        });
    }
}

/// The three layout cards, the focused one outlined with a radio dot.
void paintCards(const ModeChooser& chooser, const ChooserLayout& layout, float dp,
                ClipStack& clips) {
    const TextStyle label{labelSp * dp};
    for (std::size_t i = 0; i < layout.cards.size(); ++i) {
        const library::LibraryMode mode = library::allLibraryModes[i];
        const Rect& card = layout.cards[i];
        const RoundRect shape{card, layout.cardRadius};
        fillSoft(shape.grown(0.5f * dp), cardShadowBlurDp * dp, [](Vector2, float) {
            return cardShadow;
        });
        fillRoundRect(shape, [](Vector2, float) {
            return cardFill;
        });
        sketch(mode, card, dp, clips);

        const std::string_view name = library::label(mode);
        const float width = type().measure(name, label);
        type().drawCentred(name, card.centreX() - width * 0.5f,
                           card.bottom() - labelAboveBottomDp * dp, label, palette::ink);

        if (mode == chooser.focused()) {
            const float ring = outlineDp * dp;
            fillBand(shape, shape.grown(-ring), [](Vector2, float) {
                return outlineInk;
            });
            const Vector2 dot{card.right() - dotInsetDp * dp, card.y + dotInsetDp * dp};
            DrawCircleV(dot, dotOuterDp * dp, outlineInk);
            DrawCircleV(dot, dotInnerDp * dp, WHITE);
        }
    }
}

} // namespace

void ModeChooserPainter::paint(const ModeChooser& chooser, Vector2 size, float dp) const {
    if (!chooser.isOpen()) {
        return;
    }
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, scrim);
    const ChooserLayout layout = chooser.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    fillRoundRect(RoundRect{layout.panel, layout.panelRadius}, [](Vector2, float) {
        return panelFill;
    });
    ClipStack clips;
    const ScopedClip content{clips, layout.content};
    for (const ChooserRowBox& box : layout.rows) {
        if (box.row == ChooserRow::Cards) {
            paintCards(chooser, layout, dp, clips);
        } else {
            paintRow(box, chooser, dp);
        }
    }
}

} // namespace opensu::ui
