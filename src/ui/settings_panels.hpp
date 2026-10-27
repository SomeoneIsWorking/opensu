// settings_panels — what opens over the Settings screen: the folder chooser and the on-screen
// keyboard a path or key is typed on. Owns their state, fades and painters, so the shell composes
// one object for them and the pointer reads one hit test.
#pragma once

#include <chrono>

#include "paint.hpp"

#include "button_glyph.hpp"
#include "folder_chooser.hpp"
#include "folder_chooser_painter.hpp"
#include "on_screen_keyboard.hpp"
#include "on_screen_keyboard_painter.hpp"
#include "panel_fade.hpp"
#include "pointer_target.hpp"

namespace opensu::ui {

class SettingsPanels {
  public:
    using Clock = std::chrono::steady_clock;

    SettingsPanels(const input::Prompts& prompts, Typeface& typeface) noexcept
        : folderPainter_{prompts, typeface}, entryPainter_{prompts, typeface} {
    }

    [[nodiscard]] FolderChooser& folders() noexcept {
        return folders_;
    }
    [[nodiscard]] const FolderChooser& folders() const noexcept {
        return folders_;
    }
    /// The on-screen keyboard a path is typed on.
    [[nodiscard]] OnScreenKeyboard& entry() noexcept {
        return entry_;
    }
    [[nodiscard]] const OnScreenKeyboard& entry() const noexcept {
        return entry_;
    }

    /// Whether the folder chooser or the keyboard is up.
    [[nodiscard]] bool anyOpen() const noexcept {
        return folders_.isOpen() || entry_.isOpen();
    }

    /// Starts the fades the panels' open states call for.
    void tick(Clock::time_point now) noexcept;

    /// Draws the folder chooser and the keyboard over what is drawn; `seconds` blinks the caret.
    void drawOverlays(Canvas& canvas, Point size, float dp, Clock::time_point now,
                      double seconds) const;

    /// What the point at `point` is on while a panel is up.
    [[nodiscard]] PointerTarget pointAt(Point point, Point size, float dp) const;
    /// Moves the focus the D-pad moves to `target`, when it is one of these panels' elements.
    /// Reports whether it moved.
    bool focusTarget(const PointerTarget& target);

  private:
    /// Where the folder chooser stands: the screen above where the path keyboard docks.
    [[nodiscard]] Rect folderFrame(Point size, float dp) const;

    FolderChooser folders_;
    PanelFade folderFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    FolderChooserPainter folderPainter_;
    OnScreenKeyboard entry_;
    PanelFade entryFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    OnScreenKeyboardPainter entryPainter_;
};

} // namespace opensu::ui
