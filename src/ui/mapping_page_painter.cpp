#include "mapping_page_painter.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "hint_row.hpp"
#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"
#include "row_controls.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour cardFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour chipFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour outlineInk = rowInk;
constexpr Colour accent{0x5B, 0x4F, 0x9A, 255};
constexpr Colour accentWash{0xE7, 0xE4, 0xF4, 255};
constexpr Colour tileFill{0xF1, 0xF0, 0xF5, 255};
constexpr Colour shadow{0, 0, 0, 50};
constexpr float silhouetteShade = 0.05f;
constexpr float listeningVeil = 0.8f;
constexpr float titleSp = 19.0f;
constexpr float noteSp = 13.0f;
constexpr float calloutSp = 14.0f;
constexpr float stripSp = 14.0f;
constexpr float headerSp = 15.0f;
constexpr float tileSp = 14.0f;
constexpr float promptSp = 18.0f;
constexpr float chipRadiusDp = 10.0f;
constexpr float stripRadiusDp = 12.0f;
constexpr float popoverRadiusDp = 14.0f;
constexpr float tileRadiusDp = 9.0f;
constexpr float outlineDp = 1.8f;
constexpr float glyphDp = 22.0f;
constexpr float glyphGapDp = 4.0f;
constexpr float stripInsetDp = 14.0f;
constexpr float calloutPadDp = 6.0f;
constexpr float arrowDp = 18.0f;
constexpr float arrowGapDp = 8.0f;
constexpr float promptPadDp = 22.0f;
constexpr float promptLineDp = 30.0f;

constexpr std::array<HintPrompt, 2> boardHints{{{"A", "Change"}, {"B", "Close"}}};
constexpr std::array<HintPrompt, 2> pickHints{{{"A", "Choose"}, {"B", "Back"}}};

/// A right-pointing arrow from `x`, `length` long, centred on `centreY`.
void paintArrow(Canvas& canvas, float x, float centreY, float length, float unit, Colour ink) {
    const float width = 1.8f * unit;
    const float head = length * 0.35f;
    const Point tip{x + length, centreY};
    canvas.line(Point{x, centreY}, tip, width, ink);
    canvas.line(Point{tip.x - head, centreY - head}, tip, width, ink);
    canvas.line(Point{tip.x - head, centreY + head}, tip, width, ink);
}

/// The outline drawn round a focused box.
Rect united(const Rect& one, const Rect& two) noexcept {
    const float left = std::min(one.x, two.x);
    const float top = std::min(one.y, two.y);
    return Rect{left, top, std::max(one.right(), two.right()) - left,
                std::max(one.bottom(), two.bottom()) - top};
}

void paintFocus(Canvas& canvas, const RoundRect& box, float unit, float alpha) {
    fillBand(canvas, box, box.grown(-outlineDp * unit), [alpha](Point, float) {
        return withAlpha(outlineInk, alpha);
    });
}

void fillShape(Canvas& canvas, const RoundRect& shape, Colour colour, float alpha) {
    fillRoundRect(canvas, shape, [colour, alpha](Point, float) {
        return withAlpha(colour, alpha);
    });
}

} // namespace

float MappingPagePainter::glyphRunWidth(std::span<const std::string> keys, float box) const {
    float width = 0.0f;
    for (const std::string& key : keys) {
        width += glyphs_.advance(key, box);
    }
    return width +
           static_cast<float>(keys.empty() ? 0 : keys.size() - 1) * glyphGapDp / glyphDp * box;
}

float MappingPagePainter::paintGlyphRun(Canvas& canvas, std::span<const std::string> keys, float x,
                                        float centreY, float box, Colour ink) const {
    for (const std::string& key : keys) {
        const float advance = glyphs_.advance(key, box);
        glyphs_.paint(canvas, key, Point{x + advance * 0.5f, centreY}, box, ink);
        x += advance + glyphGapDp / glyphDp * box;
    }
    return x - (keys.empty() ? 0.0f : glyphGapDp / glyphDp * box);
}

void MappingPagePainter::paint(Canvas& canvas, const MappingPage& page, Point size, float dp,
                               const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, look.alpha));
    const MappingLayout layout = page.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    const PanelScope scope{canvas, Point{layout.card.centreX(), layout.card.centreY()}, look.scale};
    fillShape(canvas, RoundRect{layout.card, layout.radius}, cardFill, look.alpha);

    const float unit = layout.unit;
    const TextStyle title{titleSp * unit};
    typeface_.drawCentred(canvas, typeface_.fitted(page.title(), layout.title.width, title),
                          layout.title.x, layout.title.centreY(), title,
                          withAlpha(palette::ink, look.alpha));
    const TextStyle note{noteSp * unit};
    typeface_.drawCentred(canvas, typeface_.fitted(page.note(), layout.note.width, note),
                          layout.note.x, layout.note.centreY(), note,
                          withAlpha(palette::inkSoft, look.alpha));

    paintBoard(canvas, page, layout, look.alpha);
    paintStrip(canvas, page, layout, look.alpha);
    if (page.mode() == MappingMode::Listening) {
        paintListening(canvas, page, layout, look.alpha);
    }
    if (layout.popover) {
        paintPopover(canvas, page, *layout.popover, unit, look.alpha);
    }
    paintHints(canvas, page, layout, look.alpha);
}

void MappingPagePainter::paintBoard(Canvas& canvas, const MappingPage& page,
                                    const MappingLayout& layout, float alpha) const {
    const float unit = layout.unit;
    const MappingSilhouette& body = layout.silhouette;
    // Opaque, so the grips and the body do not darken where they overlap.
    const Colour shade = mix(cardFill, palette::ink, silhouetteShade);
    fillShape(canvas, RoundRect{body.leftGrip, body.gripRadius}, shade, alpha);
    fillShape(canvas, RoundRect{body.rightGrip, body.gripRadius}, shade, alpha);
    fillShape(canvas, RoundRect{body.body, body.bodyRadius}, shade, alpha);

    const bool board = page.mode() == MappingMode::Board;
    const float glyph = glyphDp * unit;
    for (std::size_t i = 0; i < page.items().size(); ++i) {
        const MappingSlot& slot = layout.slots[i];
        if (slot.side == CalloutSide::Inside) {
            continue;
        }
        const MappingItem& item = page.items()[i];
        const bool focused = i == page.focus() && page.mode() != MappingMode::Listening;
        const RoundRect chip{slot.chip, chipRadiusDp * unit};
        fillShape(canvas, chip, chipFill, alpha);
        paintGlyphRun(canvas, item.glyphs,
                      slot.chip.centreX() - glyphRunWidth(item.glyphs, glyph) * 0.5f,
                      slot.chip.centreY(), glyph, withAlpha(outlineInk, alpha));
        const Rect callout = paintCallout(canvas, item, slot, unit, alpha);
        if (focused && board) {
            // One ring round the chip and what it sends.
            const float pad = calloutPadDp * unit;
            const Rect ring = united(RoundRect{slot.chip, 0.0f}.grown(pad).rect,
                                     RoundRect{callout, 0.0f}.grown(pad).rect);
            paintFocus(canvas, RoundRect{ring, chip.radius + pad}, unit, alpha);
        } else if (focused) {
            paintFocus(canvas, chip, unit, alpha);
        }
    }
}

Rect MappingPagePainter::paintCallout(Canvas& canvas, const MappingItem& item,
                                      const MappingSlot& slot, float unit, float alpha) const {
    const TextStyle text{calloutSp * unit};
    const float glyph = glyphDp * 0.9f * unit;
    const bool showGlyphs = item.changed && !item.valueGlyphs.empty();
    const std::string words =
        showGlyphs ? std::string{} : typeface_.fitted(item.value, slot.callout.width, text);
    const float width =
        showGlyphs ? glyphRunWidth(item.valueGlyphs, glyph) : typeface_.measure(words, text);
    float x = slot.callout.x;
    if (slot.side == CalloutSide::Left) {
        x = slot.callout.right() - width;
    } else if (slot.side == CalloutSide::Above || slot.side == CalloutSide::Below) {
        x = slot.callout.centreX() - width * 0.5f;
    }
    const float centreY = slot.callout.centreY();
    const Colour ink =
        withAlpha(item.changed ? accent : palette::inkSoft, item.changed ? alpha : alpha * 0.75f);
    if (showGlyphs) {
        paintGlyphRun(canvas, item.valueGlyphs, x, centreY, glyph, ink);
    } else {
        typeface_.drawCentred(canvas, words, x, centreY, text, ink);
    }
    return Rect{x, slot.callout.y, width, slot.callout.height};
}

void MappingPagePainter::paintStrip(Canvas& canvas, const MappingPage& page,
                                    const MappingLayout& layout, float alpha) const {
    const float unit = layout.unit;
    for (std::size_t i = 0; i < page.items().size(); ++i) {
        const MappingSlot& slot = layout.slots[i];
        if (slot.side != CalloutSide::Inside) {
            continue;
        }
        const MappingItem& item = page.items()[i];
        const bool focused = i == page.focus() && page.mode() != MappingMode::Listening;
        if (item.trailing) {
            paintStripChip(canvas, item, slot.chip, unit, alpha);
            if (focused) {
                paintFocus(canvas, RoundRect{slot.chip, stripRadiusDp * unit}, unit, alpha);
            }
            continue;
        }
        const Rect shown = slot.chip.intersected(layout.chordWindow);
        if (shown.width <= 0.0f) {
            continue;
        }
        const ScopedClip clip{canvas, layout.chordWindow};
        paintStripChip(canvas, item, slot.chip, unit, alpha);
        if (focused) {
            paintFocus(canvas, RoundRect{slot.chip, stripRadiusDp * unit}, unit, alpha);
        }
    }
}

void MappingPagePainter::paintStripChip(Canvas& canvas, const MappingItem& item, const Rect& chip,
                                        float unit, float alpha) const {
    fillShape(canvas, RoundRect{chip, stripRadiusDp * unit}, chipFill, alpha);
    const TextStyle text{stripSp * unit};
    const float inset = stripInsetDp * unit;
    const float centreY = chip.centreY();
    const Colour ink = withAlpha(palette::ink, alpha);
    if (item.glyphs.empty()) {
        // Add chord, Steam Input with its setting, Reset.
        const std::string label = item.value.empty() ? item.label : item.label + " · ";
        const float room = chip.width - 2.0f * inset;
        const float labelWidth = typeface_.measure(label, text);
        const std::string value = typeface_.fitted(item.value, room - labelWidth, text);
        const float x = chip.centreX() - (labelWidth + typeface_.measure(value, text)) * 0.5f;
        typeface_.drawCentred(canvas, typeface_.fitted(label, room, text), x, centreY, text, ink);
        typeface_.drawCentred(canvas, value, x + labelWidth, centreY, text,
                              withAlpha(palette::inkSoft, alpha));
        return;
    }
    // A chord: its two buttons, an arrow, and what it does.
    const float glyph = glyphDp * unit;
    const float plusWidth = typeface_.measure(" + ", text);
    float x = chip.x + inset;
    x = paintGlyphRun(canvas, std::span{item.glyphs}.first(1), x, centreY, glyph, ink);
    typeface_.drawCentred(canvas, " + ", x, centreY, text, withAlpha(palette::inkSoft, alpha));
    x += plusWidth;
    x = paintGlyphRun(canvas, std::span{item.glyphs}.subspan(1), x, centreY, glyph, ink);
    x += arrowGapDp * unit;
    paintArrow(canvas, x, centreY, arrowDp * unit, unit, withAlpha(palette::inkSoft, alpha));
    x += (arrowDp + arrowGapDp) * unit;
    const Colour sends = withAlpha(accent, alpha);
    if (!item.valueGlyphs.empty()) {
        paintGlyphRun(canvas, item.valueGlyphs, x, centreY, glyph * 0.9f, sends);
    } else {
        typeface_.drawCentred(canvas, typeface_.fitted(item.value, chip.right() - inset - x, text),
                              x, centreY, text, sends);
    }
}

void MappingPagePainter::paintPopover(Canvas& canvas, const MappingPage& page,
                                      const MappingPopover& popover, float unit,
                                      float alpha) const {
    const RoundRect box{popover.box, popoverRadiusDp * unit};
    const RoundRect below{
        Rect{popover.box.x, popover.box.y + 4.0f * unit, popover.box.width, popover.box.height},
        box.radius};
    fillSoft(canvas, below.grown(2.0f * unit), 8.0f * unit, [alpha](Point, float) {
        return withAlpha(shadow, alpha);
    });
    fillShape(canvas, box, chipFill, alpha);
    fillBand(canvas, box, box.grown(-1.0f * unit), [alpha](Point, float) {
        return withAlpha(rowOff, alpha);
    });

    // The header: what is being changed, and an arrow to the tiles.
    const MappingPick& pick = page.picker();
    const Colour ink = withAlpha(palette::ink, alpha);
    const TextStyle heading{headerSp * unit};
    const float glyph = glyphDp * unit;
    const float centreY = popover.header.centreY();
    float x = popover.header.x;
    if (pick.glyphs.empty()) {
        typeface_.drawCentred(canvas, pick.label, x, centreY, heading, ink);
        x += typeface_.measure(pick.label, heading);
    } else {
        for (std::size_t i = 0; i < pick.glyphs.size(); ++i) {
            if (i > 0) {
                typeface_.drawCentred(canvas, " + ", x, centreY, heading,
                                      withAlpha(palette::inkSoft, alpha));
                x += typeface_.measure(" + ", heading);
            }
            x = paintGlyphRun(canvas, std::span{pick.glyphs}.subspan(i, 1), x, centreY, glyph, ink);
        }
    }
    paintArrow(canvas, x + arrowGapDp * unit, centreY, arrowDp * unit, unit,
               withAlpha(palette::inkSoft, alpha));

    const ScopedClip clip{canvas, popover.body};
    const TextStyle label{tileSp * unit};
    const float inset = 10.0f * unit;
    for (std::size_t i = 0; i < pick.choices.size(); ++i) {
        const Rect& tile = popover.tiles[i];
        if (tile.intersected(popover.body).height <= 0.0f) {
            continue;
        }
        const MappingChoice& choice = pick.choices[i];
        const bool current = i == pick.current;
        const RoundRect shape{tile, tileRadiusDp * unit};
        fillShape(canvas, shape, current ? accentWash : tileFill, alpha);
        const Colour tint = withAlpha(current ? accent : outlineInk, alpha);
        if (choice.glyphs.empty()) {
            const std::string text =
                typeface_.fitted(choice.label, tile.width - 2.0f * inset, label);
            typeface_.drawCentred(canvas, text,
                                  tile.centreX() - typeface_.measure(text, label) * 0.5f,
                                  tile.centreY(), label, tint);
        } else {
            paintGlyphRun(canvas, choice.glyphs,
                          tile.centreX() - glyphRunWidth(choice.glyphs, glyph) * 0.5f,
                          tile.centreY(), glyph, tint);
        }
        if (i == page.choice()) {
            paintFocus(canvas, shape, unit, alpha);
        }
    }
}

void MappingPagePainter::paintListening(Canvas& canvas, const MappingPage& page,
                                        const MappingLayout& layout, float alpha) const {
    const float unit = layout.unit;
    const Rect dimmed{layout.board.x, layout.board.y, layout.board.width,
                      layout.chordWindow.bottom() - layout.board.y};
    canvas.fillRect(onCanvas(dimmed), withAlpha(cardFill, alpha * listeningVeil));

    const TextStyle text{promptSp * unit};
    const TextStyle hint{noteSp * unit};
    const float glyph = glyphDp * 0.85f * unit;
    const std::array<std::string, 1> cancelKey{"B"};
    const std::string cancels = " cancels";
    const float hintWidth = glyphRunWidth(cancelKey, glyph) + typeface_.measure(cancels, hint);
    const float pad = promptPadDp * unit;
    const float width = std::max(typeface_.measure(page.prompt(), text), hintWidth) + 2.0f * pad;
    const float height = 2.0f * promptLineDp * unit + pad;
    const Rect panel{dimmed.centreX() - width * 0.5f, dimmed.centreY() - height * 0.5f, width,
                     height};
    const RoundRect shape{panel, popoverRadiusDp * unit};
    fillShape(canvas, shape, chipFill, alpha);
    fillBand(canvas, shape, shape.grown(-1.0f * unit), [alpha](Point, float) {
        return withAlpha(rowOff, alpha);
    });
    const float first = panel.y + pad * 0.5f + promptLineDp * unit * 0.5f;
    typeface_.drawCentred(canvas, page.prompt(),
                          panel.centreX() - typeface_.measure(page.prompt(), text) * 0.5f, first,
                          text, withAlpha(palette::ink, alpha));
    const float second = first + promptLineDp * unit;
    const float x = paintGlyphRun(canvas, cancelKey, panel.centreX() - hintWidth * 0.5f, second,
                                  glyph, withAlpha(outlineInk, alpha));
    typeface_.drawCentred(canvas, cancels, x, second, hint, withAlpha(palette::inkSoft, alpha));
}

void MappingPagePainter::paintHints(Canvas& canvas, const MappingPage& page,
                                    const MappingLayout& layout, float alpha) const {
    if (page.mode() == MappingMode::Listening) {
        return;
    }
    const std::span<const HintPrompt> hints =
        page.mode() == MappingMode::Picking ? std::span<const HintPrompt>{pickHints} : boardHints;
    paintHintRow(canvas, glyphs_, typeface_, hints,
                 Point{layout.card.centreX(), layout.hints.centreY()}, layout.unit,
                 withAlpha(outlineInk, alpha));
}

} // namespace opensu::ui
