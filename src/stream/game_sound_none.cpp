#include <stdexcept>

#include "game_sound.hpp"

namespace opensu::stream {

class GameSound::Sink {};

GameSound::GameSound(Setup /*setup*/) {
    throw std::runtime_error{"this system cannot capture a game's sound"};
}

GameSound::~GameSound() = default;

std::vector<std::string> GameSound::environment() const {
    return {};
}

} // namespace opensu::stream
