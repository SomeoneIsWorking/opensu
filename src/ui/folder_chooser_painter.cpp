#include "folder_chooser_painter.hpp"

#include <string>
#include <utility>
#include <vector>

#include "clip_stack.hpp"
#include "hud.hpp"
#include "panel_scope.hpp"
#include "round_shape.hpp"
#include "row_controls.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color panelFill{0xF8, 0xF8, 0xFB, 255};
constexpr Color cardFill{0xFF, 0xFF, 0xFF, 255};
constexpr float titleSp = 19.0f;
constexpr float pathSp = 13.5f;
constexpr float rowSp = 16.5f;
constexpr float hintSp = 13.0f;
constexpr float hintGlyphDp = 18.0f;
constexpr float hintGapDp = 5.0f;
constexpr float hintSpacingDp = 14.0f;
constexpr float insetDp = 14.0f;
constexpr float outlineDp = 1.8f;

} // namespace

void FolderChooserPainter::paint(const FolderChooser& chooser, Vector2 size, float dp,
                                 const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, alpha));
    const FolderChooserLayout layout = chooser.layout(Rect{0.0f, 0.0f, size.x, size.y}, dp);
    const PanelScope scope{Vector2{layout.panel.centreX(), layout.panel.centreY()}, look.scale};
    fillRoundRect(RoundRect{layout.panel, layout.radius}, [alpha](Vector2, float) {
        return withAlpha(panelFill, alpha);
    });
    const TextStyle title{titleSp * dp};
    type().drawCentred(type().fitted(chooser.title(), layout.title.width, title), layout.title.x,
                       layout.title.centreY(), title, withAlpha(palette::ink, alpha));
    const TextStyle path{pathSp * dp};
    const std::string shown =
        chooser.problem().empty() ? chooser.current().string() : chooser.problem();
    type().drawCentred(
        type().fitted(shown, layout.path.width, path), layout.path.x, layout.path.centreY(), path,
        withAlpha(chooser.problem().empty() ? palette::inkSoft : Colour{0xB3, 0x26, 0x1E, 255},
                  alpha));

    ClipStack clips;
    {
        const ScopedClip list{clips, layout.list};
        const TextStyle row{rowSp * dp};
        for (std::size_t i = 0; i < chooser.entries().size(); ++i) {
            const Rect& box = layout.rows[i];
            if (box.bottom() < layout.list.y || box.y > layout.list.bottom()) {
                continue;
            }
            const RoundRect bar{box, layout.rowRadius};
            fillRoundRect(bar, [alpha](Vector2, float) {
                return withAlpha(cardFill, alpha);
            });
            const FolderEntry& entry = chooser.entries()[i];
            const float x = box.x + insetDp * dp;
            const bool folder = entry.kind == FolderEntryKind::Folder;
            type().drawCentred(type().fitted(entry.label + (folder ? "/" : ""),
                                             box.width - 3.0f * insetDp * dp, row),
                               x, box.centreY(), row, withAlpha(palette::ink, alpha));
            if (entry.kind != FolderEntryKind::Use && entry.kind != FolderEntryKind::Clear) {
                paintChevron(box.right() - insetDp * dp, box.centreY(), dp);
            }
            if (i == chooser.focus()) {
                fillBand(bar, bar.grown(-outlineDp * dp), [alpha](Vector2, float) {
                    return withAlpha(rowInk, alpha);
                });
            }
        }
    }

    const TextStyle hint{hintSp * dp};
    const float glyph = hintGlyphDp * dp;
    const FolderEntryKind focused = chooser.entries()[chooser.focus()].kind;
    const char* press = "Open";
    if (focused == FolderEntryKind::Use) {
        press = "Use";
    } else if (focused == FolderEntryKind::Type) {
        press = "Type";
    } else if (focused == FolderEntryKind::Clear) {
        press = "Clear";
    }
    const std::vector<std::pair<const char*, const char*>> hints{{"A", press}, {"B", "Cancel"}};
    float total = 0.0f;
    for (const auto& [key, label] : hints) {
        total += glyphs_.advance(key, glyph) + hintGapDp * dp + type().measure(label, hint) +
                 hintSpacingDp * dp;
    }
    float x = layout.hints.centreX() - (total - hintSpacingDp * dp) * 0.5f;
    for (const auto& [key, label] : hints) {
        const float advance = glyphs_.advance(key, glyph);
        glyphs_.paint(key, Vector2{x + advance * 0.5f, layout.hints.centreY()}, glyph,
                      withAlpha(rowInk, alpha));
        x += advance + hintGapDp * dp;
        type().drawCentred(label, x, layout.hints.centreY(), hint, withAlpha(rowInk, alpha));
        x += type().measure(label, hint) + hintSpacingDp * dp;
    }
}

} // namespace opensu::ui
