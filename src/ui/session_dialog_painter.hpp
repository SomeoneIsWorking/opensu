// session_dialog_painter — draws the session dialog as a card over the dimmed screen, after the
// launch panel's card: the title, the wrapped paragraphs and two pill buttons with their pad
// glyphs, the focused one filled.
#pragma once

#include "button_glyph.hpp"
#include "panel_fade.hpp"
#include "session_dialog.hpp"

namespace opensu::ui {

class SessionDialogPainter {
  public:
    explicit SessionDialogPainter(const input::Prompts& prompts) noexcept : glyphs_{prompts} {
    }

    /// Where the card and its buttons stand in a `size` frame; what a pointer hits.
    [[nodiscard]] SessionDialogLayout layout(const SessionDialog& dialog, Vector2 size,
                                             float dp) const;

    /// Draws `dialog` into a `size` frame at `dp` pixels per dp, as faded and scaled by `look`.
    void paint(const SessionDialog& dialog, Vector2 size, float dp, const PanelLook& look) const;

  private:
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui
