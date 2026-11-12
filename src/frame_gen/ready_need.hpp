// ready_need — how long after a real frame arrives a generator has its first frame between ready,
// from the times its recent real frames took: the pacer plans the first generated frame at least
// that late (`framegen::CFramePacer::SetReadyNeededNs`).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace opensu::frame_gen {

/// Real frames whose ready times are kept: about two seconds at 30 fps, long enough to hold a slow
/// frame, short enough to follow the GPU's clocks.
inline constexpr std::size_t readyWindow = 64;
/// Above the window's slowest frame: a waiter's wake and a GPU clock step.
inline constexpr std::uint64_t readyHeadroomNs = 1'000'000;
/// The most the need falls per window of headroom, so a falling need moves the pacer's held
/// delay at most once a window.
inline constexpr std::uint64_t readyLowerStepNs = 2'000'000;
/// The least need: a generator never has its frame the moment the real one arrives.
inline constexpr std::uint64_t readyFloorNs = 1'000'000;

class ReadyNeed {
  public:
    /// A real frame whose first generated frame was ready `sampleNs` after it arrived, on a display
    /// refreshing every `refreshNs`. A slower frame than the need raises it at once; the need falls
    /// only after a whole window under it, by at most `readyLowerStepNs`.
    void add(std::uint64_t sampleNs, std::uint64_t refreshNs);
    /// The ready time to plan for: a whole refresh until the window is full.
    [[nodiscard]] std::uint64_t needNs(std::uint64_t refreshNs) const noexcept;
    /// Forgets every frame, as a new session starts.
    void reset() noexcept;

  private:
    [[nodiscard]] std::uint64_t slowest() const noexcept;

    std::array<std::uint64_t, readyWindow> samples_{};
    std::size_t count_{0};
    std::size_t next_{0};
    std::optional<std::uint64_t> need_;
    std::size_t sinceChange_{0};
};

} // namespace opensu::frame_gen
