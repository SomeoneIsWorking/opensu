// debounce — iiSU's repeat rule for UI sounds: an effect with a debounce is dropped when the same
// effect fired less than that long ago (`xp8.java:1559-1586`).
#pragma once

#include <array>
#include <chrono>
#include <optional>

#include "effect.hpp"

namespace iideck::audio {

class Debounce {
  public:
    using Clock = std::chrono::steady_clock;

    /// Whether `effect` plays at `now`; a played effect starts its debounce.
    [[nodiscard]] bool admit(Effect effect, Clock::time_point now);

  private:
    std::array<std::optional<Clock::time_point>, allEffects.size()> last_{};
};

} // namespace iideck::audio
