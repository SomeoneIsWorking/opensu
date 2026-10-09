// search_controller — the buttons and keys of the search panel. A pad walks the drawn keyboard; a
// physical keyboard types into the field. The text goes into the view's search, so the grid
// behind narrows as it is typed, and closing the panel leaves the results showing.
#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "preferences.hpp"
#include "ui/search_panel.hpp"

namespace opensu::app {

class SearchController {
  public:
    struct Hooks {
        /// Shows the shelf again, focusing `focus`, after the search text changed.
        std::function<void(std::size_t focus)> reshow;
        /// The tiles the grid shows now, as lines of the results list.
        std::function<std::vector<ui::SearchResult>()> results;
        /// Focuses tile `index` and presses A on it.
        std::function<void(std::size_t index)> openTile;
    };

    SearchController(ui::SearchPanel& panel, audio::SoundPlayer& sounds, Preferences& preferences,
                     Hooks hooks)
        : panel_{panel}, sounds_{sounds}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the panel on the search in use.
    void open();
    /// A pad button while the panel is up: it takes them all.
    void act(gamepad::Button button);

    /// What a physical keyboard typed: characters go into the field.
    void typeText(std::string_view text);
    void backspace();
    /// Enter: opens the focused result, or closes the panel.
    void confirm();
    /// Escape: closes the panel, leaving the search applied.
    void dismiss();
    /// Up and Down: walk the results.
    void walk(ui::Direction direction);

    /// Drops the search and shows the unnarrowed shelf. Reports whether there was one.
    bool clear();
    /// Whether a search is applied, which is when its results are what the grid shows.
    [[nodiscard]] bool searching() const noexcept {
        return !preferences_.values().view.search.empty();
    }

  private:
    void close();
    /// The field changed: search again and list the results.
    void edited();
    /// Opens result `index`: closes the panel and presses A on its tile.
    void openResult(std::size_t index);
    void moved(bool changed);
    void applyPress(const ui::SearchPress& press);

    ui::SearchPanel& panel_;
    audio::SoundPlayer& sounds_;
    Preferences& preferences_;
    Hooks hooks_;
};

} // namespace opensu::app
