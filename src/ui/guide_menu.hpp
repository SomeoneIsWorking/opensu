// guide_menu — the menu Guide opens: a panel down the left edge, after Steam's main menu, with
// openSU's own entries. Outside a game it goes to Home, Library, each store, Devices and Settings;
// over a running game it resumes or closes the game first. Power opens a second list of its own.
// Pure state, so it is tested without a window.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "library/game.hpp"
#include "panel_frame.hpp"

namespace opensu::ui {

enum class GuideAction : std::uint8_t {
    Resume,
    CloseGame,
    Home,
    Library,
    /// A store's section of Library, named by `GuideEntry::store`.
    Store,
    Devices,
    Settings,
    /// Opens the power list.
    Power,
    Sleep,
    Restart,
    ShutDown,
    QuitToDesktop,
};

struct GuideEntry {
    GuideAction action{GuideAction::Resume};
    std::string label;
    /// The store a `Store` entry goes to.
    library::Source store{library::Source::Steam};
};

/// What the menu is opened over.
struct GuideContext {
    /// A game is running; its title heads the menu.
    bool inGame{false};
    std::string title;
    /// The stores that have a section of their own.
    std::vector<library::Source> stores;
};

/// The panel down the left edge and its entry rows, in pixels.
struct GuideLayout {
    Rect panel;
    float padding{};
    std::vector<Rect> rows;

    /// The row under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
};

/// The layout of `rows` entries in `frame`; `chrome.heading` is the title line's height and
/// `chrome.footer` the room the hints take under the rows.
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
    bool move(int delta) noexcept;
    /// Focuses entry `index`; reports whether focus changed.
    bool focusEntry(std::size_t index) noexcept;

    [[nodiscard]] bool inPower() const noexcept {
        return power_;
    }
    /// Shows the power list, or the main list again.
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
    bool armed_{false};
    bool open_{false};
};

} // namespace opensu::ui
