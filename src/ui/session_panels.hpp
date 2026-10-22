// session_panels — the cards that ask the player before openSU changes how it runs: the consent
// dialog (installing session mode, a plugin's permissions), the on-screen keyboard the
// administrator password is typed on, and the first-run integration chooser. Owns their state,
// fades and painters, so the shell composes one object for them and the pointer reads one hit
// test.
#pragma once

#include <chrono>

#include "paint.hpp"

#include "integration_chooser.hpp"
#include "integration_chooser_painter.hpp"
#include "on_screen_keyboard.hpp"
#include "on_screen_keyboard_painter.hpp"
#include "panel_fade.hpp"
#include "pointer_target.hpp"
#include "session_dialog.hpp"
#include "session_dialog_painter.hpp"

namespace opensu::ui {

class SessionPanels {
  public:
    using Clock = std::chrono::steady_clock;

    SessionPanels(const input::Prompts& prompts, Typeface& typeface) noexcept
        : dialogPainter_{prompts, typeface}, passwordPainter_{prompts, typeface},
          chooserPainter_{prompts, typeface} {
    }

    [[nodiscard]] SessionDialog& dialog() noexcept {
        return dialog_;
    }
    [[nodiscard]] const SessionDialog& dialog() const noexcept {
        return dialog_;
    }
    /// The on-screen keyboard the password is typed on.
    [[nodiscard]] OnScreenKeyboard& password() noexcept {
        return password_;
    }
    [[nodiscard]] const OnScreenKeyboard& password() const noexcept {
        return password_;
    }

    /// The first-run integration chooser.
    [[nodiscard]] IntegrationChooser& integrations() noexcept {
        return chooser_;
    }
    [[nodiscard]] const IntegrationChooser& integrations() const noexcept {
        return chooser_;
    }

    [[nodiscard]] bool anyOpen() const noexcept {
        return dialog_.isOpen() || password_.isOpen() || chooser_.isOpen();
    }

    /// Starts the fades the panels' open states call for.
    void tick(Clock::time_point now) noexcept;
    /// Draws what is open into a `size` frame at `dp` pixels per dp; `seconds` blinks the caret.
    void draw(Canvas& canvas, Point size, float dp, Clock::time_point now, double seconds) const;

    /// The dialog button or keyboard key under `point`, or nothing.
    [[nodiscard]] PointerTarget pointAt(Point point, Point size, float dp) const;
    /// Moves the focus the D-pad moves to `target`, when it is one of these panels' elements;
    /// reports whether it moved.
    bool focusTarget(const PointerTarget& target);

  private:
    SessionDialog dialog_;
    PanelFade dialogFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    SessionDialogPainter dialogPainter_;
    OnScreenKeyboard password_;
    PanelFade passwordFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    OnScreenKeyboardPainter passwordPainter_;
    IntegrationChooser chooser_;
    PanelFade chooserFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    IntegrationChooserPainter chooserPainter_;
};

} // namespace opensu::ui
