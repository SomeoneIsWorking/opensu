#include <stdexcept>

#include "game_typing.hpp"

namespace opensu::app {

bool typesIntoGames(bool /*insideGamescope*/, const EmbedSocket& /*embed*/) noexcept {
    return false;
}

std::unique_ptr<GameTyping> openGameTyping(const std::string& /*x11Display*/,
                                           EmbedSocket& /*embed*/) {
    throw std::runtime_error{"openSU cannot type into games on this system"};
}

} // namespace opensu::app
