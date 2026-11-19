// pyrowave_fit — whether a guest keeps up with PyroWave: over each `pyroWaveFitWindowUs`, the share
// of its PyroWave frames that never came whole (a datagram lost costs the whole frame) or were
// skipped for decoding late. Past `pyroWaveFitLimit` the guest stops offering PyroWave for the
// session, so the host codes a hardware codec with its parity instead. Pure state with the time
// passed in.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace opensu::stream {

/// How long a stretch of PyroWave frames is judged over, and the share lost or late it may have.
inline constexpr std::uint64_t pyroWaveFitWindowUs = 5'000'000;
inline constexpr double pyroWaveFitLimit = 0.05;
/// The frames a stretch needs before it is judged.
inline constexpr std::size_t pyroWaveFitLeast = 60;

class PyroWaveFit {
  public:
    /// A guest's frame counts since its stream began.
    struct Counts {
        std::size_t decoded{0};
        std::size_t dropped{0};
        std::size_t late{0};
    };

    /// The counts at `nowUs` while PyroWave is shown; true once, when a stretch lost or skipped
    /// more than `pyroWaveFitLimit` of its frames.
    [[nodiscard]] bool take(Counts totals, std::uint64_t nowUs);
    /// Another codec is shown: the next counts start a new stretch.
    void restart() noexcept {
        start_.reset();
    }
    /// The share lost or late in the stretch that gave PyroWave up.
    [[nodiscard]] double missed() const noexcept {
        return missed_;
    }

  private:
    struct Mark {
        std::uint64_t atUs{0};
        Counts counts;
    };
    std::optional<Mark> start_;
    bool givenUp_{false};
    double missed_{0};
};

} // namespace opensu::stream
