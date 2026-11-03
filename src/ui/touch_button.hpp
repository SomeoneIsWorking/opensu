// touch_button — the round glass buttons the top bar shows while the touch controls are up: Back,
// and the menu (the Guide menu a pad opens with Guide). Shaped as iiSU's glass icon button
// (xe4.i: a 52 dp glass circle around a 24 dp icon).
#pragma once

#include <cstdint>

#include "glass.hpp"
#include "paint.hpp"

namespace opensu::ui {

enum class TouchButton : std::uint8_t { Back, Menu };

/// The button's width and height, in dp; past the 48 dp a finger needs.
inline constexpr float touchButtonDp = 52.0f;

class TouchButtonPainter {
  public:
    /// Draws `button` filling `body`; `lit` while the pointer rests on it.
    void paint(Canvas& canvas, TouchButton button, const Rect& body, float dp, bool lit) const;

  private:
    GlassPainter glass_;
};

} // namespace opensu::ui
