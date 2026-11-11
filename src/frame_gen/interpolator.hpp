// interpolator — one generator of frames between real ones, on a `GeneratorDevice`: real frames
// of a session go in as images, and each generated frame is written into an output image at a
// time between the last two. The images are RGBA8 in GENERAL layout, kept alive by the caller
// until the submission that uses them has finished.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "frame_gen/generator_gpu.hpp"
#include "framegen/LosslessGenerator.h"

namespace opensu::frame_gen {

class Interpolator {
  public:
    Interpolator() = default;
    virtual ~Interpolator() = default;
    Interpolator(const Interpolator&) = delete;
    Interpolator& operator=(const Interpolator&) = delete;
    Interpolator(Interpolator&&) = delete;
    Interpolator& operator=(Interpolator&&) = delete;

    /// A session of `width` by `height`: every real frame before it forgotten.
    [[nodiscard]] virtual bool begin(std::uint32_t width, std::uint32_t height) = 0;
    /// The session's next real frame; its submission, or nothing when none was made.
    [[nodiscard]] virtual std::optional<framegen::Submission_t>
    add(const framegen::GeneratorImage_t& frame) = 0;
    /// The scene at `t` in (0, 1) between the last two real frames, written into `output` with
    /// `hooks` recorded around it; its submission, or nothing when none was made.
    [[nodiscard]] virtual std::optional<framegen::Submission_t>
    generate(float t, const framegen::GeneratorImage_t& output,
             const framegen::OutputHooks_t& hooks) = 0;
};

/// The LSFG 3 shaders of the player's Lossless.dll (`settings.pModules` or `settings.sDllPath`)
/// through the Gamescope fork's `framegen::CLosslessGenerator`; nothing with why when the device or
/// the modules cannot generate.
[[nodiscard]] std::unique_ptr<Interpolator>
lossless(GeneratorDevice& gpu, const framegen::LosslessSettings_t& settings, std::string& why);

} // namespace opensu::frame_gen
