#include "downloads_page_painter.hpp"

#include <algorithm>
#include <string>

#include "hud.hpp"
#include "round_shape.hpp"
#include "row_controls.hpp"

namespace opensu::ui {
namespace {

constexpr Colour panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr Colour buttonFill{0xE6, 0xE4, 0xEE, 255};
/// The toast's error red.
constexpr Colour failedInk{0xb3, 0x26, 0x1e, 255};
constexpr float titleSp = 17.0f;
constexpr float lineSp = 12.5f;
constexpr float buttonSp = 14.0f;
constexpr float emptySp = 17.0f;
constexpr float outlineDp = 1.8f;
constexpr float rowRadiusDp = 12.0f;
constexpr float titleTopDp = 20.0f;
constexpr float lineTopDp = 41.0f;
constexpr float buttonPaddingDp = 10.0f;
/// An ended row is drawn this much fainter.
constexpr float endedFade = 0.7f;

Colour faded(Colour colour, float alpha) {
    return withAlpha(colour, alpha);
}

} // namespace

void DownloadsPagePainter::paintRow(Canvas& canvas, const DownloadsEntry& entry,
                                    const DownloadsLayout& layout, std::size_t index,
                                    const DownloadsPage& page, float dp, float alpha) const {
    const Rect& rect = layout.rows[index];
    const Rect& body = layout.bodies[index];
    const RoundRect bar{rect, rowRadiusDp * dp};
    fillRoundRect(canvas, bar, [alpha](Point, float) {
        return faded(cardFill, alpha);
    });
    const float ink = entry.ended ? alpha * endedFade : alpha;
    const TextStyle title{titleSp * dp};
    const TextStyle line{lineSp * dp};
    typeface_.drawCentred(canvas, typeface_.fitted(entry.title, body.width, title), body.x,
                          rect.y + titleTopDp * dp, title, faded(palette::ink, ink));

    // "Steam · Downloading", then the figures in the quieter ink.
    const std::string lead = entry.source + "  ·  " + entry.state;
    const Colour leadInk = entry.failed ? failedInk : palette::ink;
    const float lineY = rect.y + lineTopDp * dp;
    typeface_.drawCentred(canvas, lead, body.x, lineY, line, faded(leadInk, ink));
    if (!entry.figures.empty()) {
        const float after = body.x + typeface_.measure(lead, line);
        const std::string figures = "  ·  " + entry.figures;
        typeface_.drawCentred(
            canvas, typeface_.fitted(figures, std::max(body.right() - after, 0.0f), line), after,
            lineY, line, faded(entry.failed ? failedInk : palette::inkSoft, ink));
    }

    if (entry.fraction) {
        const Rect& track = layout.bars[index];
        canvas.fillRoundRect(onCanvas(track), cornerRadius(track, 1.0f), faded(rowOff, ink));
        const float filled = std::clamp(static_cast<float>(*entry.fraction), 0.0f, 1.0f);
        if (filled > 0.0f) {
            const Rect done{track.x, track.y, std::max(track.height, track.width * filled),
                            track.height};
            canvas.fillRoundRect(onCanvas(done), cornerRadius(done, 1.0f), faded(rowInk, ink));
        }
    }

    const bool rowFocused = index == page.row();
    const TextStyle button{buttonSp * dp};
    for (std::size_t i = 0; i < layout.actions[index].size() && i < entry.actions.size(); ++i) {
        const Rect& cell = layout.actions[index][i];
        const RoundRect pill{cell, cell.height * 0.5f};
        fillRoundRect(canvas, pill, [alpha](Point, float) {
            return faded(buttonFill, alpha);
        });
        const std::string text =
            typeface_.fitted(entry.actions[i], cell.width - 2.0f * buttonPaddingDp * dp, button);
        typeface_.drawCentred(canvas, text, cell.centreX() - typeface_.measure(text, button) * 0.5f,
                              cell.centreY(), button, faded(palette::ink, alpha));
        if (rowFocused && i == page.action()) {
            fillBand(canvas, pill, pill.grown(-outlineDp * dp), [alpha](Point, float) {
                return faded(rowInk, alpha);
            });
        }
    }
    if (rowFocused) {
        fillBand(canvas, bar, bar.grown(-outlineDp * dp), [alpha](Point, float) {
            return faded(rowInk, alpha);
        });
    }
}

void DownloadsPagePainter::paint(Canvas& canvas, const DownloadsPage& page,
                                 const DownloadsLayout& layout, Point size, float dp,
                                 float alpha) const {
    if (alpha <= 0.0f) {
        return;
    }
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, faded(palette::ground, alpha));
    fillRoundRect(canvas, RoundRect{layout.panel, layout.radius}, [alpha](Point, float) {
        return faded(panelFill, alpha);
    });
    const std::vector<DownloadsEntry>& entries = page.entries();
    if (entries.empty()) {
        const TextStyle text{emptySp * dp};
        const std::string empty = "No downloads";
        typeface_.drawCentred(canvas, empty,
                              layout.panel.centreX() - typeface_.measure(empty, text) * 0.5f,
                              layout.panel.centreY(), text, faded(palette::inkSoft, alpha));
        return;
    }
    const ScopedClip content{canvas, layout.panel};
    for (std::size_t i = 0; i < entries.size() && i < layout.rows.size(); ++i) {
        const Rect& rect = layout.rows[i];
        if (rect.bottom() < layout.panel.y || rect.y > layout.panel.bottom()) {
            continue;
        }
        paintRow(canvas, entries[i], layout, i, page, dp, alpha);
    }
}

} // namespace opensu::ui
