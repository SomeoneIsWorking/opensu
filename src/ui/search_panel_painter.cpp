#include "search_panel_painter.hpp"

#include "hud.hpp"
#include "paint.hpp"
#include "round_shape.hpp"

namespace opensu::ui {
namespace {

constexpr float resultSp = 16.0f;
constexpr float insetDp = 14.0f;

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
    const SearchLayout layout =
        layoutSearch(Rect{0.0f, 0.0f, size.x, size.y}, dp, panel.keyboard().style());
    const bool inResults = panel.zone() == SearchZone::Results;
    const float inset = insetDp * dp;
    // The results: as many rows as fit, from the first in view.
    const auto results = [&](float alpha) {
        const TextStyle result{resultSp * dp};
        if (panel.results().empty() && !panel.keyboard().text().empty()) {
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
            keyboard_.drawFitted(canvas, entry.title, box.x + inset, box.centreY(),
                                 box.width - detail - inset * 3.0f, result,
                                 withAlpha(palette::ink, alpha));
        }
    };
    keyboard_.paint(
        canvas, panel.keyboard(), layout.keyboard, size, dp, look, seconds,
        KeyboardExtras{.reserved = results, .hints = hints(panel), .keysFocused = !inResults});
}

} // namespace opensu::ui
