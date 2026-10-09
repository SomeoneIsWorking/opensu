#include "play_history.hpp"

namespace opensu::library {

void PlayHistory::record(const std::string& id, std::chrono::system_clock::time_point at) {
    entries_[id] = std::chrono::duration_cast<std::chrono::seconds>(at.time_since_epoch()).count();
}

void PlayHistory::apply(std::vector<Game>& games) const {
    for (Game& game : games) {
        const auto found = entries_.find(game.id);
        if (found == entries_.end()) {
            continue;
        }
        const std::chrono::system_clock::time_point recorded{std::chrono::seconds{found->second}};
        if (!game.lastPlayed || *game.lastPlayed < recorded) {
            game.lastPlayed = recorded;
        }
    }
}

} // namespace opensu::library
