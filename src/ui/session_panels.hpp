// session_panels — the session install flow's two panels: the dialog that explains it and the
// on-screen keyboard the administrator password is typed on. Owns their state, fades and painters,
// so the shell composes one object for them and the pointer reads one hit test.
#pragma once

#include <chrono>

#include "raylib.h"

#include "panel_fade.hpp"
#include "pointer_target.hpp"
#include "search_panel.hpp"
#include "search_panel_painter.hpp"
#include "session_dialog.hpp"
#include "session_dialog_painter.hpp"

namespace opensu::ui {

class SessionPanels {
  public:
    using Clock = std::chrono::steady_clock;

    SessionPanels(const input::Prompts& prompts, Typeface& typeface) noexcept
        : dialogPainter_{prompts, typeface}, passwordPainter_{prompts, typeface} {
    }

    [[nodiscard]] SessionDialog& dialog() noexcept {
        return dialog_;
    }
    [[nodiscard]] const SessionDialog& dialog() const noexcept {
        return dialog_;
    }
    /// The on-screen keyboard the password is typed on.
    [[nodiscard]] SearchPanel& password() noexcept {
        return password_;
    }
    [[nodiscard]] const SearchPanel& password() const noexcept {
        return password_;
    }

    [[nodiscard]] bool anyOpen() const noexcept {
        return dialog_.isOpen() || password_.isOpen();
    }

    /// Starts the fades the panels' open states call for.
    void tick(Clock::time_point now) noexcept;
    /// Draws what is open into a `size` frame at `dp` pixels per dp; `seconds` blinks the caret.
    void draw(Vector2 size, float dp, Clock::time_point now, double seconds) const;

    /// The dialog button or keyboard key under `point`, or nothing.
    [[nodiscard]] PointerTarget pointAt(Vector2 point, Vector2 size, float dp) const;
    /// Moves the focus the D-pad moves to `target`, when it is one of these panels' elements;
    /// reports whether it moved.
    bool focusTarget(const PointerTarget& target);

  private:
    SessionDialog dialog_;
    PanelFade dialogFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    SessionDialogPainter dialogPainter_;
    SearchPanel password_;
    PanelFade passwordFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    SearchPanelPainter passwordPainter_;
};

} // namespace opensu::ui
