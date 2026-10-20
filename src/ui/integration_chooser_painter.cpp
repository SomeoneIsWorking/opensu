#include "integration_chooser_painter.hpp"

#include <string>
#include <vector>

#include "hud.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr Colour scrim{0, 0, 0, 115};
constexpr Colour cardFill{0xF8, 0xF8, 0xFB, 255};
constexpr Colour outlineInk{0x4D, 0x46, 0x55, 255};
constexpr float titleSp = 24.0f;
constexpr float introSp = 14.0f;
constexpr float buttonSp = 17.0f;
constexpr float glyphDp = 20.0f;
constexpr float glyphGapDp = 8.0f;
constexpr float buttonRadiusDp = 20.0f;
constexpr const char* title = "Choose your integrations";
constexpr const char* doneLabel = "Done";

} // namespace

IntegrationChooserLayout IntegrationChooserPainter::layout(const IntegrationChooser& chooser,
                                                           Point size, float dp) const {
    const TextStyle body{introSp * dp};
    const std::vector<std::string> lines =
        typeface_.wrapped(intro, chooserIntroWidth(size.x, dp), body);
    return layoutIntegrationChooser(
        PanelFrame{size.x, size.y, dp}, typeface_.lineBox(TextStyle{titleSp * dp}),
        static_cast<float>(lines.size()) * typeface_.lineBox(body), chooser.options().size());
}

void IntegrationChooserPainter::paint(Canvas& canvas, const IntegrationChooser& chooser, Point size,
                                      float dp, const PanelLook& look) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const float alpha = look.alpha;
    canvas.fillRect(gpu2d::Rect{0.0f, 0.0f, size.x, size.y}, withAlpha(scrim, alpha));
    const IntegrationChooserLayout frame = layout(chooser, size, dp);
    const PanelScope scope{canvas, Point{frame.card.centreX(), frame.card.centreY()}, look.scale};
    fillRoundRect(canvas, RoundRect{frame.card, frame.radius}, [alpha](Point, float) {
        return withAlpha(cardFill, alpha);
    });

    const TextStyle heading{titleSp * dp};
    typeface_.drawCentred(canvas, title,
                          frame.card.centreX() - typeface_.measure(title, heading) * 0.5f,
                          frame.titleY, heading, withAlpha(palette::ink, alpha));
    const TextStyle body{introSp * dp};
    const float lineBox = typeface_.lineBox(body);
    float y = frame.intro.y;
    for (const std::string& line : typeface_.wrapped(intro, frame.intro.width, body)) {
        typeface_.drawCentred(canvas, line,
                              frame.card.centreX() - typeface_.measure(line, body) * 0.5f,
                              y + lineBox * 0.5f, body, withAlpha(palette::inkSoft, alpha));
        y += lineBox;
    }

    const std::vector<SettingsRow> rows = chooser.rows();
    for (std::size_t i = 0; i < rows.size() && i < frame.rows.size(); ++i) {
        rows_.paintRow(canvas, rows[i], frame.rows[i], i == chooser.focus(), dp, alpha);
    }

    const bool focused = chooser.onDone();
    const RoundRect shape{frame.done, buttonRadiusDp * dp};
    fillRoundRect(canvas, shape, [focused, alpha](Point, float) {
        return withAlpha(focused ? palette::ink : Colour{0xEE, 0xED, 0xF3, 255}, alpha);
    });
    const Colour ink = focused ? white : palette::ink;
    const TextStyle label{buttonSp * dp};
    const float glyph = glyphDp * dp;
    const float advance = glyphs_.advance("A", glyph);
    const float text = typeface_.measure(doneLabel, label);
    float x = frame.done.centreX() - (advance + glyphGapDp * dp + text) * 0.5f;
    glyphs_.paint(canvas, "A", Point{x + advance * 0.5f, frame.done.centreY()}, glyph,
                  withAlpha(focused ? ink : outlineInk, alpha));
    x += advance + glyphGapDp * dp;
    typeface_.drawCentred(canvas, doneLabel, x, frame.done.centreY(), label, withAlpha(ink, alpha));
}

} // namespace opensu::ui
