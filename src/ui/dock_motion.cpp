#include "dock_motion.hpp"

#include <algorithm>

#include "tile_motion.hpp"

namespace iideck::ui {

float DockVisibility::at(const Slide& slide, double nowMs) {
    const float target = slide.heading ? 1.0f : 0.0f;
    const double duration = slide.heading ? showMs : hideMs;
    const double t = std::clamp((nowMs - slide.startedMs) / duration, 0.0, 1.0);
    const double eased = slide.heading ? motion::linearOutSlowIn(t) : motion::fastOutLinearIn(t);
    return slide.from + (target - slide.from) * static_cast<float>(eased);
}

DockVisibility::Slide DockVisibility::settled(double nowMs) const {
    if (!slide_.heading || pinned_ || nowMs < revealUntilMs_) {
        return slide_;
    }
    const double ranOut = std::max(revealUntilMs_, slide_.startedMs);
    return Slide{at(slide_, ranOut), ranOut, false};
}

float DockVisibility::progress(double nowMs) const {
    return at(settled(nowMs), nowMs);
}

void DockVisibility::turn(bool heading, double nowMs) {
    if (slide_.heading != heading) {
        slide_ = Slide{at(slide_, nowMs), nowMs, heading};
    }
}

void DockVisibility::setPinned(bool pinned, double nowMs) {
    slide_ = settled(nowMs);
    pinned_ = pinned;
    turn(pinned_ || nowMs < revealUntilMs_, nowMs);
}

void DockVisibility::reveal(double nowMs) {
    slide_ = settled(nowMs);
    revealUntilMs_ = nowMs + revealMs;
    turn(true, nowMs);
}

IconPop::IconPop(bool selected) noexcept
    : selected_{selected}, from_{selected ? restSelected : 1.0f} {
}

void IconPop::select(bool selected, double nowMs) noexcept {
    if (selected == selected_) {
        return;
    }
    from_ = scale(nowMs);
    selected_ = selected;
    changedAtMs_ = nowMs;
}

float IconPop::scale(double nowMs) const noexcept {
    const double since = std::max(nowMs - changedAtMs_, 0.0);
    if (!selected_) {
        const auto t = static_cast<float>(std::min(since / releaseMs, 1.0));
        // guess: the release easing is unrecovered; ease-out like the grid's focus scale.
        return from_ + (1.0f - from_) * motion::easeOutCubic(t);
    }
    if (since < riseMs) {
        // guess: the pop's easing is unrecovered; ease-out like the grid's focus scale.
        return from_ + (peak - from_) * motion::easeOutCubic(static_cast<float>(since / riseMs));
    }
    const auto t = static_cast<float>(std::min((since - riseMs) / settleMs, 1.0));
    return peak + (restSelected - peak) * motion::easeOutCubic(t);
}

} // namespace iideck::ui
