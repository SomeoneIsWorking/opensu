// search_panel_painter — draws the Global Search panel: a pale card with the field and white result
// rows, the focused one outlined in ink, above the docked on-screen keyboard
// (`on_screen_keyboard_painter`).
#pragma once

#include "on_screen_keyboard_painter.hpp"
#include "panel_fade.hpp"
#include "search_panel.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class SearchPanelPainter {
  public:
    SearchPanelPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : prompts_{&prompts}, typeface_{typeface}, keyboard_{prompts, typeface} {
    }

    /// Draws `panel` into a frame of `size` at `dp` pixels per dp, as faded and scaled by `look`;
    /// `seconds` blinks the caret.
    void paint(Canvas& canvas, const SearchPanel& panel, Point size, float dp,
               const PanelLook& look, double seconds) const;

  private:
    /// The hints along the foot, which name the results zone.
    [[nodiscard]] std::vector<HintPrompt> hints(const SearchPanel& panel) const;

    const input::Prompts* prompts_;
    Typeface& typeface_;
    OnScreenKeyboardPainter keyboard_;
};

} // namespace opensu::ui
