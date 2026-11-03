// import_picks — which of the games found to import the player has chosen: one switch per game,
// all of them, none of them, and Undo of each change back to how the list stood before it.
#pragma once

#include <cstddef>
#include <vector>

#include "library/own_games.hpp"

namespace opensu::app {

class ImportPicks {
  public:
    /// Every game in `found` starts chosen.
    explicit ImportPicks(std::vector<library::own::Entry> found);

    [[nodiscard]] const std::vector<library::own::Entry>& found() const noexcept {
        return found_;
    }
    [[nodiscard]] bool chosen(std::size_t index) const {
        return chosen_.at(index);
    }
    [[nodiscard]] std::size_t chosenCount() const noexcept;
    /// Whether a change can be undone.
    [[nodiscard]] bool undoable() const noexcept {
        return !history_.empty();
    }

    /// Chooses game `index`, or leaves it out.
    void choose(std::size_t index, bool chosen);
    void chooseAll();
    void chooseNone();
    /// Puts the choice back to how it stood before the last change; false when there was none.
    bool undo();

    /// The games chosen, in the order found.
    [[nodiscard]] std::vector<library::own::Entry> picked() const;

  private:
    /// Makes `next` the choice, kept for Undo when it differs.
    void change(std::vector<bool> next);

    std::vector<library::own::Entry> found_;
    std::vector<bool> chosen_;
    std::vector<std::vector<bool>> history_;
};

} // namespace opensu::app
