#include "frame_stages.hpp"

#include <algorithm>
#include <format>
#include <string>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

constexpr std::array<const char*, FrameStages::stageCount> stageNames{
    "core", "audio", "fence", "acquire", "record", "present", "sleep"};

double milliseconds(FrameStages::Clock::duration spent) {
    return std::chrono::duration<double, std::milli>(spent).count();
}

/// p50/p95/p99 of `values`, sorting them.
std::string percentiles(std::vector<double>& values) {
    if (values.empty()) {
        return "-";
    }
    std::ranges::sort(values);
    const auto at = [&values](double share) {
        return values[std::min(values.size() - 1, static_cast<std::size_t>(
                                                      share * static_cast<double>(values.size())))];
    };
    return std::format("{:.1f}/{:.1f}/{:.1f}", at(0.5), at(0.95), at(0.99));
}

} // namespace

void FrameStages::add(Stage stage, Clock::duration spent) {
    stages_[static_cast<std::size_t>(stage)].push_back(milliseconds(spent));
}

void FrameStages::endFrame(Clock::time_point start, double audioQueuedMs) {
    if (lastStart_ != Clock::time_point{}) {
        frames_.push_back(milliseconds(start - lastStart_));
    } else {
        windowStart_ = start;
    }
    lastStart_ = start;
    audio_.push_back(audioQueuedMs);
    if (start - windowStart_ >= window) {
        log(start - windowStart_);
        windowStart_ = start;
    }
}

void FrameStages::log(Clock::duration over) {
    const double seconds = std::chrono::duration<double>(over).count();
    std::string line =
        std::format("{} frames in {:.1f} s ({:.1f}/s), ms p50/p95/p99: frame {}", frames_.size(),
                    seconds, static_cast<double>(frames_.size()) / seconds, percentiles(frames_));
    for (std::size_t i = 0; i < stageCount; ++i) {
        line += std::format(", {} {}", stageNames[i], percentiles(stages_[i]));
        stages_[i].clear();
    }
    line += std::format(", audio queued {}", percentiles(audio_));
    lucent::info("retro", "{}", line);
    frames_.clear();
    audio_.clear();
}

} // namespace opensu::retro
