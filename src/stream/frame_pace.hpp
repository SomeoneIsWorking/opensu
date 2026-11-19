// frame_pace — which captured frames a host codes for a guest that asked for fewer frames a
// second than are captured: each frame is due at a running schedule of the asked rate, met by the
// first capture within half a capture of it, so 45 from 60 sends three captures in four and 30
// every other. Pure state with the times passed in.
#pragma once

#include <cstdint>

namespace opensu::stream {

class FramePace {
  public:
    /// Whether the frame captured at `capturedUs` is due at `framesPerSecond` (0 for every frame)
    /// with `capturedPerSecond` captured.
    [[nodiscard]] bool due(std::uint64_t capturedUs, std::uint8_t framesPerSecond,
                           int capturedPerSecond) const noexcept;
    /// The frame captured at `capturedUs` was sent, moving the schedule a frame on.
    void sent(std::uint64_t capturedUs, std::uint8_t framesPerSecond) noexcept;

  private:
    /// When the next frame is due; 0 before the first sent at a rate.
    std::uint64_t dueUs_{0};
};

} // namespace opensu::stream
