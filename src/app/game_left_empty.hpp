// game_left_empty — whether a hosted game nobody plays any more closes. One started for a guest
// does once its last guest leaves. The player's own game does once its own seat was taken over by a
// device and that device left cleanly (its openSU closed, or End remote play from the Guide), with
// no guest still in it. A guest that drops is held (`seatHold`) and leaves nothing here.
#pragma once

#include <cstddef>

namespace opensu::app {

class GameLeftEmpty {
  public:
    /// A guest left cleanly; `own` when it played the seat of the player here.
    void left(bool own) noexcept {
        ownSeatLeft_ = ownSeatLeft_ || own;
    }
    /// With `guests` still in it, whether the game closes; `forGuest` when it was started for one.
    [[nodiscard]] bool closes(bool forGuest, std::size_t guests) const noexcept {
        return guests == 0 && (forGuest || ownSeatLeft_);
    }

  private:
    bool ownSeatLeft_{false};
};

} // namespace opensu::app
