// guide_menu — the menu Guide opens: a panel down the left edge, after Steam's main menu, with
// openSU's own entries. Outside a game it goes to Home, Library, Devices and Settings; over a
// running game it resumes or closes the game first. A power button in the bottom-left corner,
// reached by moving down past the last row, opens a second list of its own: Sleep, Restart, Shut
// down, then the way to openSU's session mode (or back to the desktop from it). Pure state, so it
// is tested without a window.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "panel_frame.hpp"

namespace opensu::ui {

enum class GuideAction : std::uint8_t {
    Resume,
    CloseGame,
    Home,
    Library,
    Devices,
    Settings,
    Sleep,
    Restart,
    ShutDown,
    QuitToDesktop,
    SwitchToDesktop,
    SwitchToSession,
    InstallSession,
};

struct GuideEntry {
    GuideAction action{GuideAction::Resume};
    std::string label;
};

/// What the menu is opened over.
struct GuideContext {
    /// A game is running; its title heads the menu.
    bool inGame{false};
    std::string title;
    /// openSU is the login session: the power list ends with Switch to desktop, not Quit.
    bool loginSession{false};
    /// Session mode is installed: the power list offers Switch to session mode, else Install
    /// session mode. Outside the login session only.
    bool sessionInstalled{false};
};

/// The panel down the left edge and its entry rows, in pixels.
struct GuideLayout {
    Rect panel;
    float padding{};
    std::vector<Rect> rows;
    /// The bottom row: the power button at its left end, the hints at its right; the rows stop
    /// above it.
    Rect footer;
    Rect powerButton;

    /// The row under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
};

/// The layout of `rows` entries in `frame`; `chrome.heading` is the title line's height and
/// `chrome.footer` the height of the stacked hints beside the power button.
[[nodiscard]] GuideLayout layoutGuide(const PanelFrame& frame, const PanelChrome& chrome,
                                      std::size_t rows) noexcept;

class GuideMenu {
  public:
    /// Opens the main list over `context`, focused on its first entry.
    void open(GuideContext context);
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    /// The panel's heading: the game's title, or openSU's name; "Power" in the power list.
    [[nodiscard]] std::string title() const;
    [[nodiscard]] std::span<const GuideEntry> entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    [[nodiscard]] const GuideEntry& selected() const noexcept {
        return entries_[focus_];
    }
    /// The entry's label as the panel reads it: an armed one asks for a second press.
    [[nodiscard]] std::string shown(std::size_t index) const;

    /// Moves focus by `delta` entries, stopping at either end; moving lets go of an armed entry.
    /// Down past the last row of the main list focuses the power button, and Up leaves it.
    bool move(int delta) noexcept;
    /// Focuses entry `index`; reports whether focus changed.
    bool focusEntry(std::size_t index) noexcept;
    /// Focuses the power button; reports whether focus changed. Not in the power list.
    bool focusPower() noexcept;
    /// Whether focus is on the power button rather than a row.
    [[nodiscard]] bool powerFocused() const noexcept {
        return powerFocused_;
    }

    [[nodiscard]] bool inPower() const noexcept {
        return power_;
    }
    /// Shows the power list, or the main list again with the power button focused.
    void showPower();
    void showMain();

    /// Asks the focused entry for a second press, which a restart or shut down needs.
    void arm() noexcept;
    [[nodiscard]] bool armed() const noexcept {
        return armed_;
    }
    /// What B does here, which the panel's hint says.
    [[nodiscard]] const char* backLabel() const noexcept;

  private:
    void build();

    GuideContext context_;
    std::vector<GuideEntry> entries_;
    std::size_t focus_{0};
    bool power_{false};
    bool powerFocused_{false};
    bool armed_{false};
    bool open_{false};
};

} // namespace opensu::ui
