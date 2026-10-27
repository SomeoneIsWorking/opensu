#include "screen_stack.hpp"

#include <algorithm>
#include <vector>

#include "interface_theme.hpp"

namespace opensu::ui {

void ScreenStack::layout(Clock::time_point now, const gpu2d::Rect& window, float covered, float dp,
                         Chrome chrome) {
    chrome_ = chrome;
    navigator_.setTheme(interfaceTheme(dp));
    navigator_.setTime(std::chrono::duration<double>(now.time_since_epoch()).count());
    navigator_.layout(window, covered);
    if (!navigator_.visible()) {
        images_.clear();
    }
}

bool ScreenStack::paged() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return !screen->overlay();
    });
}

void ScreenStack::paintPages(Canvas& canvas) {
    navigator_.paintPages(canvas);
}

void ScreenStack::paintOverlays(Canvas& canvas) {
    navigator_.paintOverlays(canvas);
}

} // namespace opensu::ui
