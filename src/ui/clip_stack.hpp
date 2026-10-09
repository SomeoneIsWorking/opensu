// clip_stack — nested clipping for painters. raylib's scissor does not nest: ending an inner one
// turns clipping off, so a painter that clips inside a clipped panel pushes and pops here instead.
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

class ClipStack {
  public:
    /// Applies a clip rect, or nothing to stop clipping.
    using Apply = std::function<void(std::optional<Rect>)>;

    /// Clips through raylib's scissor.
    ClipStack();
    explicit ClipStack(Apply apply);
    ClipStack(const ClipStack&) = delete;
    ClipStack& operator=(const ClipStack&) = delete;
    /// Stops clipping if anything is still pushed.
    ~ClipStack();

    /// Clips to `area` within whatever is clipped already.
    void push(const Rect& area);
    /// Restores the clip that stood before the last push.
    void pop();

  private:
    Apply apply_;
    std::vector<Rect> stack_;
};

/// Pushes on construction and pops on destruction.
class ScopedClip {
  public:
    ScopedClip(ClipStack& clips, const Rect& area);
    ScopedClip(const ScopedClip&) = delete;
    ScopedClip& operator=(const ScopedClip&) = delete;
    ~ScopedClip();

  private:
    ClipStack& clips_;
};

} // namespace opensu::ui
