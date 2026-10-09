// direction_repeat — a held direction moves once, then repeats, the same for keys and pads.
#pragma once

#include <chrono>
#include <optional>

#include "event.hpp"

namespace opensu::gamepad {

class DirectionRepeat {
  public:
    using Clock = std::chrono::steady_clock;

    /// iiSU takes Android's system repeats and accepts one per 95 ms: a held d-pad moves again
    /// after the repeat timeout, then every second 50 ms repeat (input-sound.md §1.3).
    static constexpr std::chrono::milliseconds delay{400};
    static constexpr std::chrono::milliseconds interval{100};

    /// True for Up, Down, Left and Right, the buttons this repeats.
    [[nodiscard]] static bool repeats(Button button);

    /// A newly held direction replaces the one before it.
    void press(Button direction, Clock::time_point now);
    void release(Button direction);

    /// The direction to move now, if one is due.
    [[nodiscard]] std::optional<Button> poll(Clock::time_point now);

  private:
    Button held_{Button::None};
    /// The press not yet answered with a move.
    Button pending_{Button::None};
    Clock::time_point next_{};
};

} // namespace opensu::gamepad
