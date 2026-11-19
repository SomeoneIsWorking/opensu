// playout_delay — how long after its send a guest shows a frame, so a link's stalls cost a steady
// delay instead of hitches: the 98th percentile of the last `playoutWindowUs` of
// frames' send-to-decoded times, taken at once when it grows and let shrink `playoutShrinkUs` a
// refresh, so a burst that came behind a stall plays out one frame a refresh. Pure state with the
// times passed in, on the guest's clock.
#pragma once

#include <cstdint>
#include <deque>

namespace opensu::stream {

/// The stretch of decoded frames the delay is judged over.
inline constexpr std::uint64_t playoutWindowUs = 3'000'000;
/// The share of those frames the delay waits for.
inline constexpr double playoutShare = 0.98;
/// How far the delay shrinks a refresh once fewer frames need it.
inline constexpr std::uint64_t playoutShrinkUs = 1000;
/// The longest delay: about what a guest holds (`framesHeldMost` at 60 a second), past which it
/// shows its oldest frame anyway.
inline constexpr std::uint64_t playoutMostUs = 200'000;

class PlayoutDelay {
  public:
    /// A frame sent at `sentUs` was decoded at `decodedUs`.
    void decoded(std::uint64_t sentUs, std::uint64_t decodedUs);
    /// The delay for this refresh at `nowUs`, which moves it toward what the window wants.
    [[nodiscard]] std::uint64_t refresh(std::uint64_t nowUs);
    /// The delay the last refresh gave.
    [[nodiscard]] std::uint64_t delayUs() const noexcept {
        return delayUs_;
    }

  private:
    struct Sample {
        std::uint64_t decodedUs{0};
        std::uint64_t lateUs{0};
    };
    std::deque<Sample> samples_;
    std::uint64_t delayUs_{0};
};

} // namespace opensu::stream
