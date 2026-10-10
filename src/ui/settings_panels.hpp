// settings_panels — the Settings screen and what opens over it: the folder chooser and the
// on-screen keyboard a path is typed on. Owns their state, fades and painters, so the shell
// composes one object for them and the pointer reads one hit test.
#pragma once

#include <chrono>

#include "raylib.h"

#include "button_glyph.hpp"
#include "folder_chooser.hpp"
#include "folder_chooser_painter.hpp"
#include "page_panel.hpp"
#include "panel_fade.hpp"
#include "pointer_target.hpp"
#include "search_panel.hpp"
#include "search_panel_painter.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

class SettingsPanels {
  public:
    using Clock = std::chrono::steady_clock;

    SettingsPanels(const input::Prompts& prompts, Typeface& typeface) noexcept
        : page_{typeface}, folderPainter_{prompts, typeface}, entryPainter_{prompts, typeface} {
    }

    [[nodiscard]] SettingsPage& page() noexcept {
        return page_.page();
    }
    [[nodiscard]] const SettingsPage& page() const noexcept {
        return page_.page();
    }
    [[nodiscard]] FolderChooser& folders() noexcept {
        return folders_;
    }
    [[nodiscard]] const FolderChooser& folders() const noexcept {
        return folders_;
    }
    /// The on-screen keyboard a path is typed on.
    [[nodiscard]] SearchPanel& entry() noexcept {
        return entry_;
    }
    [[nodiscard]] const SearchPanel& entry() const noexcept {
        return entry_;
    }

    /// Whether the page, or a panel over it, is up.
    [[nodiscard]] bool anyOpen() const noexcept {
        return page_.isOpen() || folders_.isOpen() || entry_.isOpen();
    }
    /// Whether the page is up with nothing over it.
    [[nodiscard]] bool onlyPage() const noexcept {
        return page_.isOpen() && !folders_.isOpen() && !entry_.isOpen();
    }
    /// Whether the page is drawn, which includes fading out.
    [[nodiscard]] bool pageVisible(Clock::time_point now) const noexcept {
        return page_.visible(now);
    }

    /// Starts the fades the panels' open states call for.
    void tick(Clock::time_point now) noexcept;

    /// Draws the page between `topInset` and `bottomInset` in a `size` frame at `dp` pixels per dp.
    void drawPage(Vector2 size, float dp, float topInset, float bottomInset,
                  Clock::time_point now) const;
    /// Draws the folder chooser and the keyboard over what is drawn; `seconds` blinks the caret.
    void drawOverlays(Vector2 size, float dp, Clock::time_point now, double seconds) const;

    /// What the point at `point` is on while the page or a panel over it is up.
    [[nodiscard]] PointerTarget pointAt(Vector2 point, Vector2 size, float dp, float topInset,
                                        float bottomInset) const;
    /// Moves the focus the D-pad moves to `target`, when it is one of these panels' elements.
    /// Reports whether it moved.
    bool focusTarget(const PointerTarget& target);

  private:
    PagePanel page_;
    FolderChooser folders_;
    PanelFade folderFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    FolderChooserPainter folderPainter_;
    SearchPanel entry_;
    PanelFade entryFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    SearchPanelPainter entryPainter_;
};

} // namespace opensu::ui
