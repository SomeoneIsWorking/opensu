// library_additions — the add tiles Library shows and what each does: Add a game where this OS can
// start a game of the player's own (`library::addGameOffer`), Add a ROM folder (Settings' own
// flow), Import from a PC on your network once the LAN's ROM import fills its hook, and Set up
// <integration> for each one this build offers that the player has not turned on, which opens its
// switch in Settings › Integrations. An integration's tile goes once it is on.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "integrations_in_force.hpp"
#include "library/shelf.hpp"

namespace opensu::app {

class LibraryAdditions {
  public:
    struct Hooks {
        /// Add a game: a file, Steam's shortcuts, Lutris.
        std::function<void()> addGame;
        /// Library › Add a ROM folder.
        std::function<void()> addRomFolder;
        /// The LAN's ROM import; its tile is left out while this is unset.
        std::function<void()> importFromLan;
        /// Settings › Integrations with the switch `row` focused.
        std::function<void(const std::string& row)> setUp;
    };

    LibraryAdditions(const IntegrationsInForce& integrations, Hooks hooks)
        : integrations_{integrations}, hooks_{std::move(hooks)} {
    }

    /// The tiles, in Library's order.
    [[nodiscard]] std::vector<library::AddTile> tiles() const;
    /// Does what the tile `id` stands for.
    void press(const std::string& id) const;

  private:
    const IntegrationsInForce& integrations_;
    Hooks hooks_;
};

} // namespace opensu::app
