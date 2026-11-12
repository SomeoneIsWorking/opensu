// paced_frames — frame generation for frames that arrive in CPU RGBA: each real frame uploaded to
// the generator's device, the frames between two generated at the times its pacer
// (`framegen::CFramePacer`) plans and read back, and for each present the real frame or the
// generated one it shows. The remote play guest and the runner keep their frames' own details
// beside it, by the real frame's index.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/interpolator.hpp"
#include "frame_gen/ready_need.hpp"
#include "frame_gen/ready_watch.hpp"
#include "framegen/FramePacer.h"
#include "settings/framegen_settings.hpp"

namespace opensu::frame_gen {

/// Real frames whose images and outputs are kept: the interval shown, the one after, and the next.
inline constexpr std::uint64_t bufferedFrames = 3;

/// What a present shows: real frame `real`, or, when `generated` holds its RGBA, a frame
/// generated before it.
struct Shown {
    std::uint64_t real{0};
    std::optional<std::span<const std::uint8_t>> generated;
};

class PacedFrames {
  public:
    /// Frames between real ones made by `generator` on `gpu`, the content's pixels `pixel`, paced
    /// by the game's `settings`, each real frame's first generated frame planned at least as late
    /// as the generator's recent frames took to be ready (`ReadyNeed`).
    PacedFrames(GeneratorDevice& gpu, std::unique_ptr<Interpolator> generator,
                const settings::FrameGenSettings& settings, SourcePixel pixel);
    ~PacedFrames();
    PacedFrames(const PacedFrames&) = delete;
    PacedFrames& operator=(const PacedFrames&) = delete;
    PacedFrames(PacedFrames&&) = delete;
    PacedFrames& operator=(PacedFrames&&) = delete;

    /// A real frame of `width` by `height` in RGBA, ready at `readyNs` and here at `arrivalNs`,
    /// on a display refreshing every `refreshNs`: its index, which `Shown::real` names. A new
    /// size starts a session. Throws `std::runtime_error` when generation fails.
    std::uint64_t add(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                      std::uint64_t readyNs, std::uint64_t arrivalNs, std::uint64_t refreshNs);
    /// What the present at `nowNs` shows; nothing keeps the one shown.
    [[nodiscard]] std::optional<Shown> vblank(std::uint64_t nowNs, std::uint64_t refreshNs);

    [[nodiscard]] std::uint32_t width() const noexcept {
        return width_;
    }
    [[nodiscard]] std::uint32_t height() const noexcept {
        return height_;
    }
    [[nodiscard]] std::uint32_t maxGenerated() const noexcept {
        return pacer_.GetMaxGeneratedPerFrame();
    }
    [[nodiscard]] std::uint64_t realIn() const noexcept {
        return realIn_;
    }
    [[nodiscard]] std::uint64_t shown() const noexcept {
        return shown_;
    }
    [[nodiscard]] std::uint64_t shownGenerated() const noexcept {
        return shownGenerated_;
    }
    [[nodiscard]] std::uint64_t dropped() const noexcept {
        return pacer_.GetDroppedOutputs();
    }
    [[nodiscard]] std::optional<framegen::PacingState> state() const noexcept {
        return state_;
    }
    /// The counts in one line: real frames in, presents and generated frames shown, outputs
    /// dropped, the pacer's state and the ready time it plans for.
    [[nodiscard]] std::string summary() const;

  private:
    /// The submissions of a real frame's interval's generated images, and the last submission
    /// that uses its slot's image or buffers.
    struct Slot {
        std::vector<std::uint64_t> generated;
        std::uint64_t busy{0};
    };

    [[nodiscard]] std::size_t slotOf(std::uint64_t interval, std::uint32_t generated) const;
    [[nodiscard]] std::optional<std::uint64_t>
    sequenceOf(const framegen::PacedOutput_t& output) const;
    void begin(std::uint32_t width, std::uint32_t height);

    GeneratorDevice& gpu_;
    std::unique_ptr<Interpolator> generator_;
    framegen::CFramePacer pacer_;
    SourcePixel pixel_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<std::unique_ptr<GeneratorImage>> sources_;
    std::vector<std::unique_ptr<HostBuffer>> uploads_;
    std::vector<std::unique_ptr<GeneratorImage>> outputs_;
    std::vector<std::unique_ptr<HostBuffer>> readbacks_;
    std::array<Slot, bufferedFrames> slots_{};
    /// Real frames since the session began, the last of them the generator took, and its state.
    std::uint64_t frames_{0};
    std::optional<std::uint64_t> lastAdded_;
    std::optional<framegen::PacingState> state_;
    ReadyNeed need_;
    std::uint64_t needNs_{0};
    ReadyWatch watch_;
    std::uint64_t realIn_{0};
    std::uint64_t shown_{0};
    std::uint64_t shownGenerated_{0};
};

} // namespace opensu::frame_gen
