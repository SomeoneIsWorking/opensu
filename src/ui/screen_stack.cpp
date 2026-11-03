#include "screen_stack.hpp"

#include "interface_theme.hpp"

namespace opensu::ui {

void ScreenStack::layout(Clock::time_point now, const gpu2d::Rect& window, float covered,
                         Density density, Chrome chrome, bool touch) {
    chrome_ = chrome;
    navigator_.setTouch(touch);
    navigator_.setTheme(interfaceTheme(density));
    navigator_.setTime(std::chrono::duration<double>(now.time_since_epoch()).count());
    navigator_.layout(window, covered);
    if (!navigator_.visible()) {
        images_.clear();
    }
}

void ScreenStack::paintPages(Canvas& canvas) {
    navigator_.paintPages(canvas);
}

void ScreenStack::paintOverlays(Canvas& canvas) {
    navigator_.paintOverlays(canvas);
}

} // namespace opensu::ui
