// search_controller — iiSU's Global Search (`g73.b`, `kl2.a`; screens.md) on the interface toolkit:
// a card over the home screen with the search field and its results under it. It opens with the
// on-screen keyboard up; the text goes into the view's search as it is typed, so the grid behind
// narrows, and closing the card leaves the results showing. Submit (Enter, or the keyboard's Done)
// goes to the first result; A on a result closes the card and presses A on its tile.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "preferences.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

/// The most bytes a search holds.
inline constexpr std::size_t searchMostLength = 64;

/// A line of the results: a title and what it is.
struct SearchResult {
    std::string title;
    std::string detail;

    bool operator==(const SearchResult&) const = default;
};

class SearchController {
  public:
    struct Hooks {
        /// Shows the shelf again, focusing `focus`, after the search text changed.
        std::function<void(std::size_t focus)> reshow;
        /// The tiles the grid shows now, as lines of the results.
        std::function<std::vector<SearchResult>()> results;
        /// Focuses tile `index` and presses A on it.
        std::function<void(std::size_t index)> openTile;
    };

    /// What the search acts on and through.
    struct Parts {
        gpu2d::ui::Navigator& navigator;
        const ui::Chrome& chrome;
        KeyboardPad& keyboard;
        audio::SdlSoundPlayer& sounds;
        Preferences& preferences;
    };

    SearchController(Parts parts, Hooks hooks)
        : navigator_{parts.navigator}, chrome_{parts.chrome}, keyboard_{parts.keyboard},
          sounds_{parts.sounds}, preferences_{parts.preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the card on the search in use, with the keyboard up.
    void open();
    /// Closes the card, leaving the search applied.
    void close();
    [[nodiscard]] bool isOpen() const;

    /// Drops the search and shows the unnarrowed shelf. Reports whether there was one.
    bool clear();
    /// Whether a search is applied, which is when its results are what the grid shows.
    [[nodiscard]] bool searching() const noexcept {
        return !text().empty();
    }

    // What the card asks.
    [[nodiscard]] const std::string& text() const noexcept {
        return preferences_.values().view.search;
    }
    /// The field changed to `text`; a leading or doubled space is refused.
    void edit(std::string text);
    [[nodiscard]] const std::vector<SearchResult>& results() const noexcept {
        return results_;
    }
    /// Submit: the focus goes to the first result.
    void toResults();
    /// Opens result `index`: closes the card and presses A on its tile.
    void openResult(std::size_t index);

  private:
    /// Searches again and lists the results.
    void refresh();

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    Hooks hooks_;
    std::vector<SearchResult> results_;
};

} // namespace opensu::app
