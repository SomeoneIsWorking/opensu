#include "screen_stack.hpp"

#include "interface_theme.hpp"

namespace opensu::ui {

void ScreenStack::layout(Clock::time_point now, const gpu2d::Rect& window, float dp, Chrome chrome) {
    chrome_ = chrome;
    navigator_.setTheme(interfaceTheme(dp));
    navigator_.setTime(std::chrono::duration<double>(now.time_since_epoch()).count());
    navigator_.layout(window);
}

void ScreenStack::paint(Canvas& canvas) {
    if (navigator_.visible()) {
        navigator_.paint(canvas);
    }
}

} // namespace opensu::ui
