// touch_button — the round glass buttons the top bar shows while the touch controls are up: Back,
// and the menu (the Guide menu a pad opens with Guide). Shaped as iiSU's glass icon button (xe4.i:
// a glass circle around a 24 dp icon), drawn at the row's height.
#pragma once

#include <cstdint>

#include "glass.hpp"
#include "paint.hpp"

namespace opensu::ui {

enum class TouchButton : std::uint8_t { Back, Menu };

/// The width and height of the room a finger takes the button in, in dp; past the 48 dp it needs.
inline constexpr float touchButtonDp = 52.0f;

class TouchButtonPainter {
  public:
    /// Draws `button` filling `body`; `lit` while the pointer rests on it.
    void paint(Canvas& canvas, TouchButton button, const Rect& body, float dp, bool lit) const;

  private:
    GlassPainter glass_;
};

} // namespace opensu::ui
