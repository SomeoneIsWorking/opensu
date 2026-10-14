#include "game_pads.hpp"

namespace opensu::app {

std::vector<std::string> GamePads::hold(const input::ButtonMap& map) {
    chords_.setGame(map.chords());
    syncChords();
    held_ = true;
    return pads_.hold(input::forwardMap(map, chords_.shell()));
}

void GamePads::release() {
    pads_.release();
    held_ = false;
    chords_.setGame({});
    syncChords();
}

void GamePads::setBlocked(bool blocked) {
    if (held_) {
        pads_.setBlocked(blocked);
    }
}

void GamePads::syncChords() {
    pads_.setChords(chords_.triggers());
}

} // namespace opensu::app
