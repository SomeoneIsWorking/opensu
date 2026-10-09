#include "panel_fade.hpp"

#include <algorithm>

namespace opensu::ui {
namespace {

float lerp(float from, float to, float t) noexcept {
    return from + (to - from) * std::clamp(t, 0.0f, 1.0f);
}

} // namespace

float PanelFade::sinceMs(Clock::time_point now) const noexcept {
    return std::chrono::duration<float, std::milli>(now - changedAt_).count();
}

void PanelFade::show(Clock::time_point now) noexcept {
    alphaAtChange_ = alpha(now);
    scaleAtChange_ = shown_ || alphaAtChange_ > 0.0f ? scale(now) : spec_.scaleFrom;
    changedAt_ = now;
    shown_ = true;
}

void PanelFade::hide(Clock::time_point now) noexcept {
    alphaAtChange_ = alpha(now);
    scaleAtChange_ = scale(now);
    changedAt_ = now;
    shown_ = false;
}

void PanelFade::follow(bool open, Clock::time_point now) noexcept {
    if (open && !shown_) {
        show(now);
    } else if (!open && shown_) {
        hide(now);
    }
}

float PanelFade::alpha(Clock::time_point now) const noexcept {
    const float t = sinceMs(now);
    if (shown_) {
        return spec_.fadeInMs <= 0.0f ? 1.0f : lerp(alphaAtChange_, 1.0f, t / spec_.fadeInMs);
    }
    return spec_.fadeOutMs <= 0.0f ? 0.0f : lerp(alphaAtChange_, 0.0f, t / spec_.fadeOutMs);
}

float PanelFade::scale(Clock::time_point now) const noexcept {
    const float t = sinceMs(now);
    if (shown_) {
        return spec_.scaleInMs <= 0.0f ? 1.0f : lerp(scaleAtChange_, 1.0f, t / spec_.scaleInMs);
    }
    return spec_.fadeOutMs <= 0.0f ? spec_.scaleTo
                                   : lerp(scaleAtChange_, spec_.scaleTo, t / spec_.fadeOutMs);
}

bool PanelFade::visible(Clock::time_point now) const noexcept {
    return shown_ || alpha(now) > 0.0f;
}

} // namespace opensu::ui
