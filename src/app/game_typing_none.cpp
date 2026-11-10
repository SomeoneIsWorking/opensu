#include <stdexcept>

#include "game_typing.hpp"

namespace opensu::app {

bool typesIntoGames(bool /*insideGamescope*/) noexcept {
    return false;
}

std::unique_ptr<GameTyping> openGameTyping(const std::string& /*x11Display*/) {
    throw std::runtime_error{"openSU cannot type into games on this system"};
}

} // namespace opensu::app
