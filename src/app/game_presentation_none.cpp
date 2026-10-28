#include "game_presentation.hpp"

namespace opensu::app {

bool presentationOnPlatform() noexcept {
    return false;
}

std::unique_ptr<PresentationOutput> openPresentationOutput(bool /*insideGamescope*/) {
    return nullptr;
}

} // namespace opensu::app
