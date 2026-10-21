// guide_panels — the menus Guide opens, over the home screen or over a running game: the left menu
// and the quick menu on the right, and the game settings tabs that open in the quick menu's place
// (from it, or over a game's details page). Owns their state and painters, so the shell composes
// one object for them and the pointer reads one hit test.
#pragma once

#include "guide_menu.hpp"
#include "guide_menu_painter.hpp"
#include "pointer_target.hpp"
#include "quick_menu.hpp"
#include "quick_menu_painter.hpp"
#include "settings_tabs.hpp"
#include "settings_tabs_painter.hpp"

namespace opensu::ui {

class GuidePanels {
  public:
    GuidePanels(const input::Prompts& prompts, Typeface& typeface) noexcept
        : guidePainter_{prompts, typeface}, quickPainter_{prompts, typeface},
          tabsPainter_{prompts, typeface} {
    }

    [[nodiscard]] GuideMenu& guide() noexcept {
        return guide_;
    }
    [[nodiscard]] const GuideMenu& guide() const noexcept {
        return guide_;
    }
    [[nodiscard]] QuickMenu& quick() noexcept {
        return quick_;
    }
    [[nodiscard]] const QuickMenu& quick() const noexcept {
        return quick_;
    }
    [[nodiscard]] SettingsTabs& tabs() noexcept {
        return tabs_;
    }
    [[nodiscard]] const SettingsTabs& tabs() const noexcept {
        return tabs_;
    }
    [[nodiscard]] bool anyOpen() const noexcept {
        return guide_.isOpen() || quick_.isOpen() || tabs_.isOpen();
    }

    /// Draws whichever menu is open into a `size` frame at `dp` pixels per dp.
    void draw(Canvas& canvas, Point size, float dp) const;
    /// The entry, row or slider level under `point`, the backdrop outside the open menu, or
    /// nothing when no menu is open.
    [[nodiscard]] PointerTarget pointAt(Point point, Point size, float dp) const;
    /// Moves the open menu's focus to `target`, when it is one of its elements; reports whether it
    /// moved.
    bool focusTarget(const PointerTarget& target);

  private:
    GuideMenu guide_;
    GuideMenuPainter guidePainter_;
    QuickMenu quick_;
    QuickMenuPainter quickPainter_;
    SettingsTabs tabs_;
    SettingsTabsPainter tabsPainter_;
};

} // namespace opensu::ui
