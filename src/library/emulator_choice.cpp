#include "emulator_choice.hpp"

#include <algorithm>

namespace opensu::library {
namespace {

/// The pick for `game`: its own, else its system's; null when it has none.
const std::string* pickFor(const Game& game, const EmulatorChoices::Entries& games,
                           const EmulatorChoices::Entries& systems) {
    if (const auto own = games.find(game.id); own != games.end()) {
        return &own->second;
    }
    if (game.source != Source::Rom) {
        return nullptr;
    }
    const auto system = systems.find(game.sourceId);
    return system == systems.end() ? nullptr : &system->second;
}

} // namespace

void EmulatorChoices::set(const std::string& gameId, const std::string& emulator) {
    entries_.insert_or_assign(gameId, emulator);
}

void EmulatorChoices::setSystem(const std::string& system, const std::string& emulator) {
    if (emulator.empty()) {
        systems_.erase(system);
    } else {
        systems_.insert_or_assign(system, emulator);
    }
}

void EmulatorChoices::apply(std::vector<Game>& games) const {
    for (Game& game : games) {
        const std::string* pick = pickFor(game, entries_, systems_);
        if (pick == nullptr) {
            continue;
        }
        const auto option = std::ranges::find(game.emulatorOptions, *pick, &EmulatorOption::name);
        if (option == game.emulatorOptions.end()) {
            continue;
        }
        // A system's pick gives way to a core the romset matches, or waits for the romset to be
        // checked; the game's own pick does not.
        if ((option->romset == RomsetFit::Mismatch || option->romset == RomsetFit::Pending) &&
            !entries_.contains(game.id)) {
            continue;
        }
        game.emulator = option->name;
        game.launch = option->launch;
        if (option->installed) {
            game.unavailable.clear();
        } else if (!option->problem.empty()) {
            game.unavailable = option->name + " cannot start: " + option->problem;
        } else {
            game.unavailable = option->name + " is not installed";
        }
    }
}

} // namespace opensu::library
