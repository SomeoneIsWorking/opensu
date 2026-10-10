#include "session_dialog_painter.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "hud.hpp"
#include "panel_scope.hpp"
#include "round_shape.hpp"
#include "typeface.hpp"

namespace opensu::ui {
namespace {

constexpr Color scrim{0, 0, 0, 115};
constexpr Color outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float titleSp = 24.0f;
constexpr float bodySp = 16.0f;
constexpr float buttonSp = 17.0f;
constexpr float glyphDp = 20.0f;
constexpr float glyphGapDp = 8.0f;
constexpr float paragraphGapDp = 10.0f;
constexpr float buttonRadiusDp = 22.0f;

using Paragraph = std::vector<std::string>;

/// `text` broken into lines no wider than `room`, at spaces.
Paragraph wrap(const std::string& text, float room, const TextStyle& style) {
    Paragraph lines;
    std::string line;
    std::size_t at = 0;
    while (at <= text.size()) {
        std::size_t end = text.find(' ', at);
        end = end == std::string::npos ? text.size() : end;
        const std::string word = text.substr(at, end - at);
        std::string joined = line;
        if (!joined.empty()) {
            joined += ' ';
        }
        joined += word;
        if (!line.empty() && type().measure(joined, style) > room) {
            lines.push_back(line);
            line = word;
        } else {
            line = joined;
        }
        at = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<Paragraph> wrapAll(const SessionDialog& dialog, float room, float dp) {
    std::vector<Paragraph> wrapped;
    for (const std::string& paragraph : dialog.text().paragraphs) {
        wrapped.push_back(wrap(paragraph, room, TextStyle{bodySp * dp}));
    }
    return wrapped;
}

float heightOf(const std::vector<Paragraph>& wrapped, float lineBox, float dp) {
    float height = 0.0f;
    for (std::size_t i = 0; i < wrapped.size(); ++i) {
        height += static_cast<float>(wrapped[i].size()) * lineBox;
        height += i + 1 < wrapped.size() ? paragraphGapDp * dp : 0.0f;
    }
    return height;
}

void drawMiddle(const std::string& text, float centreX, float centreY, const TextStyle& style,
                Color colour) {
    type().drawCentred(text, centreX - type().measure(text, style) * 0.5f, centreY, style, colour);
}

} // namespace

SessionDialogLayout SessionDialogPainter::layout(const SessionDialog& dialog, Vector2 size,
                                                 float dp) const {
    const float lineBox = type().lineBox(TextStyle{bodySp * dp});
    const std::vector<Paragraph> wrapped = wrapAll(dialog, dialogBodyWidth(size.x, dp), dp);
    return layoutSessionDialog(PanelFrame{size.x, size.y, dp},
                               SessionDialogMetrics{type().lineBox(TextStyle{titleSp * dp}),
                                                    lineBox, heightOf(wrapped, lineBox, dp)},
                               !dialog.busy());
}

void SessionDialogPainter::paint(const SessionDialog& dialog, Vector2 size, float dp,
                                 const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    DrawRectangleRec(Rectangle{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, alpha));
    const SessionDialogLayout frame = layout(dialog, size, dp);
    const PanelScope scope{Vector2{frame.card.centreX(), frame.card.centreY()}, look.scale};
    fillRoundRect(RoundRect{frame.card, frame.radius}, [alpha](Vector2, float) {
        return withAlpha(palette::panel, alpha);
    });

    drawMiddle(dialog.text().title, frame.card.centreX(), frame.titleY, TextStyle{titleSp * dp},
               withAlpha(palette::ink, alpha));
    const TextStyle body{bodySp * dp};
    const float lineBox = type().lineBox(body);
    float y = frame.body.y;
    for (const Paragraph& lines : wrapAll(dialog, frame.body.width, dp)) {
        for (const std::string& line : lines) {
            type().drawCentred(line, frame.body.x, y + lineBox * 0.5f, body,
                               withAlpha(palette::inkSoft, alpha));
            y += lineBox;
        }
        y += paragraphGapDp * dp;
    }

    if (const std::optional<std::string>& progress = dialog.progress()) {
        drawMiddle(*progress, frame.card.centreX(), frame.progressY, body,
                   withAlpha(palette::ink, alpha));
        return;
    }
    const TextStyle label{buttonSp * dp};
    const std::array<const std::string*, 2> labels{&dialog.text().accept, &dialog.text().cancel};
    const std::array<const char*, 2> keys{"A", "B"};
    for (std::size_t i = 0; i < frame.buttons.size(); ++i) {
        const Rect& box = frame.buttons[i];
        const bool focused = i == dialog.focus();
        const RoundRect shape{box, buttonRadiusDp * dp};
        fillRoundRect(shape, [focused, alpha](Vector2, float) {
            return withAlpha(
                focused ? static_cast<Color>(palette::ink) : Color{0xEE, 0xED, 0xF3, 255}, alpha);
        });
        const Color ink = focused ? WHITE : static_cast<Color>(palette::ink);
        const float glyph = glyphDp * dp;
        const float advance = glyphs_.advance(keys[i], glyph);
        const float text = type().measure(*labels[i], label);
        float x = box.centreX() - (advance + glyphGapDp * dp + text) * 0.5f;
        glyphs_.paint(keys[i], Vector2{x + advance * 0.5f, box.centreY()}, glyph,
                      withAlpha(focused ? ink : outlineInk, alpha));
        x += advance + glyphGapDp * dp;
        type().drawCentred(*labels[i], x, box.centreY(), label, withAlpha(ink, alpha));
    }
}

} // namespace opensu::ui
