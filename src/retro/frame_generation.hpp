// frame_generation — openSU's own frame generation in the runner: each frame the core draws in
// pixels goes to `frame_gen::flow` on a generator device of its own, which works on it scaled up
// by `frame_gen::flowContentScale`, and each present at the display's refresh shows the real
// frame or one generated before it (`frame_gen::PacedFrames`), drawn through the compositor and
// the player's shaders as a real frame is. A real frame shows one core frame later than without
// it. Built where frame_gen is (Linux); elsewhere `open` says it is not.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "frame_compositor.hpp"
#include "settings/framegen_settings.hpp"

namespace opensu::retro {

class FrameGeneration {
  public:
    FrameGeneration() = default;
    virtual ~FrameGeneration() = default;
    FrameGeneration(const FrameGeneration&) = delete;
    FrameGeneration& operator=(const FrameGeneration&) = delete;
    FrameGeneration(FrameGeneration&&) = delete;
    FrameGeneration& operator=(FrameGeneration&&) = delete;

    /// Frame generation paced by `settings`; nothing with why when it cannot run here.
    [[nodiscard]] static std::unique_ptr<FrameGeneration>
    open(const settings::FrameGenSettings& settings, std::string& why);

    /// The core's frame in CPU RGBA, drawn at `nowNs` on a display refreshing every `refreshNs`;
    /// false once generation has stopped.
    [[nodiscard]] virtual bool real(const FrameImage& frame, std::uint64_t nowNs,
                                    std::uint64_t refreshNs) = 0;
    /// Waits for the frames generated so far, for a run that shows every one it can (--frame-pngs).
    virtual void settle() = 0;
    /// What the present at `nowNs` shows, its pixels kept until the next real frame; nothing keeps
    /// the one shown.
    [[nodiscard]] virtual std::optional<FrameImage> present(std::uint64_t nowNs,
                                                            std::uint64_t refreshNs) = 0;
    /// Real frames in, presents and generated frames shown, and the pacer's state.
    [[nodiscard]] virtual std::string summary() const = 0;
};

} // namespace opensu::retro
