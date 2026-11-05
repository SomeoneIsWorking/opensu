#include "shader_scale.hpp"

#include <algorithm>
#include <cmath>

#include "clock/percentile.hpp"
#include "lucent/log.h"

namespace opensu::retro {
namespace {

double milliseconds(std::chrono::nanoseconds spent) {
    return std::chrono::duration<double, std::milli>(spent).count();
}

} // namespace

ShaderScale::ShaderScale(settings::ShaderResolution mode) : mode_{mode} {
    window_.reserve(shaderScaleWindow);
}

void ShaderScale::reset(settings::ShaderResolution mode) {
    mode_ = mode;
    level_ = 0;
    calm_ = 0;
    window_.clear();
}

void ShaderScale::add(std::chrono::nanoseconds gpu, std::chrono::nanoseconds budget) {
    if (!timed() || budget <= std::chrono::nanoseconds::zero()) {
        return;
    }
    window_.push_back(milliseconds(gpu));
    if (window_.size() < shaderScaleWindow) {
        return;
    }
    std::ranges::sort(window_);
    const double median = clock::percentile(window_, 0.5);
    window_.clear();
    const double budgetMs = milliseconds(budget);
    if (median > budgetMs * shaderScaleOver) {
        calm_ = 0;
        if (level_ + 1 < shaderScales.size()) {
            step(level_ + 1, median, budgetMs);
        }
        return;
    }
    if (level_ == 0) {
        return;
    }
    const double grown = std::pow(shaderScales[level_ - 1] / shaderScales[level_], 2.0);
    calm_ = median * grown < budgetMs * shaderScaleCalm ? calm_ + 1 : 0;
    if (calm_ >= shaderScaleCalmWindows) {
        calm_ = 0;
        step(level_ - 1, median, budgetMs);
    }
}

void ShaderScale::step(std::size_t level, double medianMs, double budgetMs) {
    const char* way = level > level_ ? "down" : "back up";
    level_ = level;
    lucent::info("retro", "shader resolution {} to {}% (GPU {:.1f} ms of a {:.1f} ms frame)", way,
                 std::lround(shaderScales[level_] * 100.0F), medianMs, budgetMs);
}

} // namespace opensu::retro
