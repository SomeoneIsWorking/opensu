// shader_scale — the share of the placed rectangle's resolution a shader preset's chain is drawn
// at. Under Auto it follows each frame's GPU time against the frame's budget: a step down when a
// window's median is over budget, a step back up once a run of windows shows the step up would
// still fit. Full holds the whole resolution. Pure state; the frame compositor feeds it.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <vector>

#include "settings/video_settings.hpp"

namespace opensu::retro {

/// The scales the chain is drawn at, largest first.
inline constexpr std::array<float, 5> shaderScales{1.0F, 0.85F, 0.7F, 0.6F, 0.5F};
/// Frames whose median GPU time is judged at once.
inline constexpr std::size_t shaderScaleWindow = 30;
/// A window over this share of the budget steps down.
inline constexpr double shaderScaleOver = 0.9;
/// A window whose time, grown by the area a step up adds, stays under this share is calm.
inline constexpr double shaderScaleCalm = 0.75;
/// Calm windows in a row before a step up.
inline constexpr int shaderScaleCalmWindows = 4;

class ShaderScale {
  public:
    explicit ShaderScale(settings::ShaderResolution mode);

    /// Starts again at full resolution under `mode`.
    void reset(settings::ShaderResolution mode);

    /// Whether it follows the GPU times it is given: Auto only.
    [[nodiscard]] bool timed() const {
        return mode_ == settings::ShaderResolution::Auto;
    }

    /// One frame drawn at `scale()` took `gpu` of the GPU, its budget `budget`; may step.
    void add(std::chrono::nanoseconds gpu, std::chrono::nanoseconds budget);

    [[nodiscard]] float scale() const {
        return shaderScales[level_];
    }

  private:
    /// Moves to `level`, logging why from the window's `medianMs` and `budgetMs`.
    void step(std::size_t level, double medianMs, double budgetMs);

    settings::ShaderResolution mode_;
    std::size_t level_{0};
    int calm_{0};
    std::vector<double> window_;
};

} // namespace opensu::retro
