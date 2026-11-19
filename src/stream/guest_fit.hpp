// guest_fit — whether a guest keeps up with what the host sends, judged over each
// `guestFitWindowUs` of its frame counts. A PyroWave frame that misses one datagram is dropped
// whole and one decoded late is skipped, neither of which the host's datagram loss shows: past
// `pyroWaveFitLimit` of them, or with its frames decoded later than `pyroWaveArrivalLimitUs` after
// their send, the guest gives PyroWave up for the session, so the host codes a hardware codec with
// its parity. Past `behindLimit` of any codec's frames lost or late it asks
// the host for fewer frames a second (`frameRateSteps`, under the rate that arrived), and after
// `cleanWindows` calm windows for more again, twice as many each time a raise had to be taken back.
// A step down whose next window did not halve the share lost is taken back at once, and no step
// down is tried for `heldOffWindows` windows, twice as long each time: a Wi-Fi stall loses as much
// at 45 frames a second as at 60. Pure state with the time passed in.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace opensu::stream {

/// How long a stretch of frames is judged over, and the frames it needs to be judged.
inline constexpr std::uint64_t guestFitWindowUs = 5'000'000;
inline constexpr std::size_t guestFitLeast = 60;
/// The share of PyroWave's frames lost or skipped late it may have.
inline constexpr double pyroWaveFitLimit = 0.05;
/// How long after their send PyroWave's frames may be decoded: past it the wire time of its large
/// frames and its decode cost more than a hardware codec's frame takes.
inline constexpr std::uint64_t pyroWaveArrivalLimitUs = 25'000;
/// How soon into a stretch PyroWave's arrival is judged, sooner than its losses.
inline constexpr std::uint64_t pyroWaveArrivalJudgedUs = 2'000'000;
/// The share of frames lost or late that asks for fewer, and under which a window is calm.
inline constexpr double behindLimit = 0.10;
inline constexpr double behindCalm = 0.02;
/// The frame rates a guest steps down through; 0 is every frame the host captures. A step down
/// goes to the highest step under the rate that arrived, so 120 sent to a 60 Hz guest asks for 60.
inline constexpr std::array<std::uint8_t, 4> frameRateSteps{0, 60, 45, 30};
/// The calm windows in a row before the rate steps up, and the most that grows to.
inline constexpr int cleanWindows = 3;
inline constexpr int mostCleanWindows = 24;
/// The windows no step down is tried for after one did not help, and the most that grows to.
inline constexpr int heldOffWindows = 12;
inline constexpr int mostHeldOffWindows = 96;

/// The slower of two asked rates, 0 being every frame.
[[nodiscard]] constexpr std::uint8_t slowerRate(std::uint8_t a, std::uint8_t b) noexcept {
    if (a == 0 || b == 0) {
        return a == 0 ? b : a;
    }
    return a < b ? a : b;
}

class GuestFit {
  public:
    /// A guest's frame counts since its stream began.
    struct Counts {
        std::size_t decoded{0};
        std::size_t dropped{0};
        std::size_t late{0};
        /// How long after their send frames are decoded now (`PlayoutDelay`), not a total.
        std::uint64_t arrivalUs{0};
    };

    /// The counts at `nowUs`, PyroWave's when `pyroWave`; true when the verdict changed.
    [[nodiscard]] bool take(Counts totals, bool pyroWave, std::uint64_t nowUs);

    [[nodiscard]] bool pyroWaveGivenUp() const noexcept {
        return pyroWaveGivenUp_;
    }
    /// The frames a second to ask the host for, 0 for all.
    [[nodiscard]] std::uint8_t framesPerSecond() const noexcept {
        return frameRateSteps.at(step_);
    }
    /// The share of frames lost or late in the last window judged.
    [[nodiscard]] double missed() const noexcept {
        return missed_;
    }
    /// How long after their send frames were decoded at the last window judged.
    [[nodiscard]] std::uint64_t arrivalUs() const noexcept {
        return arrivalUs_;
    }

  private:
    struct Mark {
        std::uint64_t atUs{0};
        Counts counts;
        bool pyroWave{false};
    };
    std::optional<Mark> start_;
    bool pyroWaveGivenUp_{false};
    std::size_t step_{0};
    int calm_{0};
    int calmNeeded_{cleanWindows};
    /// The last change raised the rate, so stepping down now takes the raise back.
    bool raised_{false};
    /// The last step down, while its first window after is judged.
    struct StepDown {
        std::size_t from{0};
        double missed{0};
    };
    std::optional<StepDown> steppedDown_;
    int heldOff_{0};
    int holdFor_{heldOffWindows};
    double missed_{0};
    std::uint64_t arrivalUs_{0};
};

} // namespace opensu::stream
