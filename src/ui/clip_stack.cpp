#include "clip_stack.hpp"

#include <cmath>
#include <utility>

#include "raylib.h"

namespace opensu::ui {
namespace {

void scissor(std::optional<Rect> area) {
    if (!area) {
        EndScissorMode();
        return;
    }
    // Whole pixels that stay inside the area, so nothing bleeds past a rounded panel's edge.
    const float left = std::ceil(area->x);
    const float top = std::ceil(area->y);
    const float right = std::floor(area->right());
    const float bottom = std::floor(area->bottom());
    BeginScissorMode(static_cast<int>(left), static_cast<int>(top),
                     static_cast<int>(std::max(0.0f, right - left)),
                     static_cast<int>(std::max(0.0f, bottom - top)));
}

} // namespace

ClipStack::ClipStack() : ClipStack{scissor} {
}

ClipStack::ClipStack(Apply apply) : apply_{std::move(apply)} {
}

ClipStack::~ClipStack() {
    if (!stack_.empty()) {
        apply_(std::nullopt);
    }
}

void ClipStack::push(const Rect& area) {
    stack_.push_back(stack_.empty() ? area : stack_.back().intersected(area));
    apply_(stack_.back());
}

void ClipStack::pop() {
    if (stack_.empty()) {
        return;
    }
    stack_.pop_back();
    apply_(stack_.empty() ? std::nullopt : std::optional<Rect>{stack_.back()});
}

ScopedClip::ScopedClip(ClipStack& clips, const Rect& area) : clips_{clips} {
    clips_.push(area);
}

ScopedClip::~ScopedClip() {
    clips_.pop();
}

} // namespace opensu::ui
