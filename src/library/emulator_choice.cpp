#include "emulator_choice.hpp"

#include <algorithm>

namespace opensu::library {

void EmulatorChoices::set(const std::string& gameId, const std::string& emulator) {
    entries_.insert_or_assign(gameId, emulator);
}

void EmulatorChoices::apply(std::vector<Game>& games) const {
    for (Game& game : games) {
        const auto found = entries_.find(game.id);
        if (found == entries_.end()) {
            continue;
        }
        const auto option =
            std::ranges::find(game.emulatorOptions, found->second, &EmulatorOption::name);
        if (option == game.emulatorOptions.end()) {
            continue;
        }
        game.emulator = option->name;
        game.launch = option->launch;
        game.unavailable = option->installed ? std::string{} : option->name + " is not installed";
    }
}

} // namespace opensu::library
