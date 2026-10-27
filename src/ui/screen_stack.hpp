// screen_stack — the shell's one stack of interface-toolkit screens (Settings, and what opens over
// it), with the face, glyphs and theme they are drawn with. The shell lays it out each tick over
// the whole window and draws it under the top bar; the app pushes screens and hands it input.
#pragma once

#include <chrono>

#include "gpu2d/ui/navigator.hpp"

#include "button_glyph.hpp"
#include "interface_text.hpp"
#include "paint.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// What the shell's chrome takes from the window, which a full screen leaves room for.
struct Chrome {
    /// The top bar's height, in pixels.
    float top{};
};

class ScreenStack {
  public:
    using Clock = std::chrono::steady_clock;

    ScreenStack(const input::Prompts& prompts, Typeface& typeface)
        : glyphs_{prompts, typeface}, text_{typeface}, prompts_{glyphs_},
          navigator_{text_, prompts_, {}} {
    }

    [[nodiscard]] gpu2d::ui::Navigator& navigator() noexcept {
        return navigator_;
    }
    [[nodiscard]] const gpu2d::ui::Navigator& navigator() const noexcept {
        return navigator_;
    }
    [[nodiscard]] const Chrome& chrome() const noexcept {
        return chrome_;
    }
    /// Whether a screen is open.
    [[nodiscard]] bool active() const noexcept {
        return navigator_.active();
    }
    /// Whether anything is drawn, which includes screens fading out.
    [[nodiscard]] bool visible() const noexcept {
        return navigator_.visible();
    }

    /// Sets the clock and theme and lays the screens out over `window`, under `chrome`.
    void layout(Clock::time_point now, const gpu2d::Rect& window, float dp, Chrome chrome);
    void paint(Canvas& canvas);

  private:
    ButtonGlyphPainter glyphs_;
    InterfaceText text_;
    InterfacePrompts prompts_;
    gpu2d::ui::Navigator navigator_;
    Chrome chrome_;
};

} // namespace opensu::ui
