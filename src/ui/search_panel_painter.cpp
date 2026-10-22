#include "search_panel_painter.hpp"

#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr float resultSp = 16.0f;
constexpr float insetDp = 14.0f;
constexpr Colour cardFill{0xF8, 0xF8, 0xFB, 240};

} // namespace

std::vector<HintPrompt> SearchPanelPainter::hints(const SearchPanel& panel) const {
    const bool inResults = panel.zone() == SearchZone::Results;
    const OnScreenKeyboard& keyboard = panel.keyboard();
    if (prompts_->device.current() == input::Device::KeyboardMouse) {
        return {{"A", inResults ? "Open" : "Done"}, {"B", "Close"}};
    }
    if (inResults) {
        return {{"A", "Open"}, {"X", "Keys"}, {"+", "Close"}};
    }
    std::vector<HintPrompt> own = keyboard_.hints(keyboard);
    if (keyboard.style() == input::KeyboardStyle::DaisyWheel) {
        own.insert(own.begin() + 1, {"DOWN", "Results"});
    } else {
        own.insert(own.begin() + 2, {"X", "Results"});
    }
    return own;
}

void SearchPanelPainter::paint(Canvas& canvas, const SearchPanel& panel, Point size, float dp,
                               const PanelLook& look, double seconds) const {
    if (look.alpha <= 0.0f) {
        return;
    }
    const SearchLayout layout =
        layoutSearch(Rect{0.0f, 0.0f, size.x, size.y}, dp, panel.keyboard().style());
    const bool inResults = panel.zone() == SearchZone::Results;
    const float alpha = look.alpha;
    {
        const PanelScope scope{canvas, Point{layout.card.centreX(), layout.card.centreY()},
                               look.scale};
        const bool none = panel.results().empty() && !panel.keyboard().text().empty();
        // "No results" takes a row of its own.
        const std::size_t listed =
            none ? 1
                 : panel.results().size() - std::min(panel.results().size(), panel.firstListed());
        canvas.fillRoundRect(onCanvas(layout.cardFor(listed)), layout.cardRadius,
                             withAlpha(cardFill, alpha));
        keyboard_.paintField(canvas, panel.keyboard(), layout.field, alpha, seconds);
        const float inset = insetDp * dp;
        const TextStyle result{resultSp * dp};
        if (none) {
            typeface_.drawCentred(canvas, "No results", layout.results[0].x + inset,
                                  layout.results[0].centreY(), result,
                                  withAlpha(palette::inkSoft, alpha));
        }
        for (std::size_t row = 0; row < layout.results.size(); ++row) {
            const std::size_t index = panel.firstListed() + row;
            if (index >= panel.results().size()) {
                break;
            }
            const Rect& box = layout.results[row];
            OnScreenKeyboardPainter::paintBar(canvas, box,
                                              inResults && index == panel.resultFocus(), dp, alpha);
            const SearchResult& entry = panel.results()[index];
            const float detail = typeface_.measure(entry.detail, result);
            typeface_.drawCentred(canvas, entry.detail, box.right() - inset - detail, box.centreY(),
                                  result, withAlpha(palette::inkSoft, alpha));
            typeface_.drawCentred(
                canvas, typeface_.fitted(entry.title, box.width - detail - inset * 3.0f, result),
                box.x + inset, box.centreY(), result, withAlpha(palette::ink, alpha));
        }
    }
    keyboard_.paint(canvas, panel.keyboard(), layout.keyboard, look, seconds,
                    KeyboardExtras{.hints = hints(panel), .keysFocused = !inResults});
}

} // namespace opensu::ui
