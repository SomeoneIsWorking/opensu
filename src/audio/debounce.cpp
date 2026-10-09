#include "debounce.hpp"

namespace opensu::audio {

bool Debounce::admit(Effect effect, Clock::time_point now) {
    std::optional<Clock::time_point>& last = last_[static_cast<std::size_t>(effect)];
    if (last && now - *last < debounceOf(effect)) {
        return false;
    }
    last = now;
    return true;
}

} // namespace opensu::audio
