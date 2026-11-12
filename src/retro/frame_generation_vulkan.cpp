#include "frame_generation.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/gpu_spans.hpp"
#include "frame_gen/interpolator.hpp"
#include "frame_gen/paced_frames.hpp"
#include "lucent/count_histogram.h"
#include "lucent/log.h"
#include "lucent/thread_work_counter.h"

namespace opensu::retro {
namespace {

/// openSU's generator on `gpu` for a core's own frames; throws when it cannot run.
std::unique_ptr<frame_gen::Interpolator> flowOf(frame_gen::GeneratorDevice& gpu) {
    std::string why;
    std::unique_ptr<frame_gen::Interpolator> generator =
        frame_gen::flow(gpu, frame_gen::flowContentScale, why);
    if (!generator) {
        throw std::runtime_error{why};
    }
    return generator;
}

class FlowGeneration final : public FrameGeneration {
  public:
    // The core's frame is the content's own pixels.
    FlowGeneration(const settings::FrameGenSettings& settings, bool measuring)
        : frames_{gpu_, flowOf(gpu_), settings, frame_gen::SourcePixel{}} {
        if (measuring) {
            work_.emplace();
        }
        lucent::info("framegen",
                     "openSU's generator on {}, the core's frames scaled up by {}, up to {} per "
                     "real frame",
                     gpu_.name(), frame_gen::flowContentScale, frames_.maxGenerated());
    }
    ~FlowGeneration() override = default;
    FlowGeneration(const FlowGeneration&) = delete;
    FlowGeneration& operator=(const FlowGeneration&) = delete;
    FlowGeneration(FlowGeneration&&) = delete;
    FlowGeneration& operator=(FlowGeneration&&) = delete;

    bool real(const FrameImage& frame, std::uint64_t nowNs, std::uint64_t refreshNs) override {
        const Phase phase{work_, realWork_};
        try {
            const std::uint64_t index =
                frames_.add(frame.rgba, frame.width, frame.height, nowNs, nowNs, refreshNs);
            Real& kept = reals_[index % frame_gen::bufferedFrames];
            kept.rgba.assign(frame.rgba.begin(), frame.rgba.end());
            kept.aspect = frame.aspect;
            kept.interval = frame.interval;
            kept.addedNs = nowNs;
            kept.shown = false;
            return true;
        } catch (const std::runtime_error& failure) {
            lucent::error("framegen", "frame generation stops: {}", failure.what());
            return false;
        }
    }

    void settle() override {
        // A lost device shows in the next real frame's failure.
        static_cast<void>(gpu_.idle());
    }

    std::optional<FrameImage> present(std::uint64_t nowNs, std::uint64_t refreshNs) override {
        const Phase phase{work_, presentWork_};
        const std::optional<frame_gen::Shown> shown = frames_.vblank(nowNs, refreshNs);
        if (!shown) {
            return std::nullopt;
        }
        Real& real = reals_[shown->real % frame_gen::bufferedFrames];
        if (!shown->generated && !real.shown) {
            real.shown = true;
            const std::uint64_t late = nowNs - real.addedNs;
            lateNs_ += late;
            latestNs_ = std::max(latestNs_, late);
            ++realsShown_;
        }
        return FrameImage{.rgba =
                              shown->generated.value_or(std::span<const std::uint8_t>{real.rgba}),
                          .width = frames_.width(),
                          .height = frames_.height(),
                          .aspect = real.aspect,
                          .gpu = std::nullopt,
                          .interval = real.interval};
    }

    std::string summary() const override {
        constexpr std::uint64_t nanosPerMicro = 1000;
        const std::uint64_t mean = realsShown_ == 0 ? 0 : lateNs_ / realsShown_;
        return std::format("{}, real frames shown {} us after the core drew them on average, {} us "
                           "at most",
                           frames_.summary(), mean / nanosPerMicro, latestNs_ / nanosPerMicro);
    }

    void reportWork() const override {
        if (!work_) {
            return;
        }
        const char* unit =
            work_->unit() == lucent::perf::WorkUnit::instructions ? "instructions" : "cpu_ns";
        lucent::info("perf",
                     "framegen runner run-end: {} real frames, {} presents, {} on the "
                     "runner's thread",
                     realWork_.count(), presentWork_.count(), unit);
        frame_gen::perfLine("framegen real", realWork_, unit, "real");
        frame_gen::perfLine("framegen present", presentWork_, unit, "present");
    }

  private:
    /// Counts the work between its construction and destruction into `into` while measuring.
    class Phase {
      public:
        Phase(const std::optional<lucent::perf::ThreadWorkCounter>& counter,
              lucent::perf::CountHistogram& into)
            : counter_{counter}, into_{into}, start_{counter ? counter->read() : 0} {
        }
        ~Phase() {
            if (counter_) {
                into_.add(counter_->read() - start_);
            }
        }
        Phase(const Phase&) = delete;
        Phase& operator=(const Phase&) = delete;
        Phase(Phase&&) = delete;
        Phase& operator=(Phase&&) = delete;

      private:
        const std::optional<lucent::perf::ThreadWorkCounter>& counter_;
        lucent::perf::CountHistogram& into_;
        std::uint64_t start_;
    };

    /// A real frame's pixels and how it is shown, kept while its interval is.
    struct Real {
        std::vector<std::uint8_t> rgba;
        float aspect{0.0F};
        std::chrono::nanoseconds interval{};
        std::uint64_t addedNs{0};
        bool shown{false};
    };

    frame_gen::GeneratorDevice gpu_;
    frame_gen::PacedFrames frames_;
    std::array<Real, frame_gen::bufferedFrames> reals_{};
    /// How long after the core drew them real frames were first shown.
    std::uint64_t lateNs_{0};
    std::uint64_t latestNs_{0};
    std::uint64_t realsShown_{0};
    /// The runner thread's counter while measuring, and the work of each real frame and present.
    std::optional<lucent::perf::ThreadWorkCounter> work_;
    lucent::perf::CountHistogram realWork_;
    lucent::perf::CountHistogram presentWork_;
};

} // namespace

std::unique_ptr<FrameGeneration> FrameGeneration::open(const settings::FrameGenSettings& settings,
                                                       bool measuring, std::string& why) {
    try {
        return std::make_unique<FlowGeneration>(settings, measuring);
    } catch (const std::runtime_error& failure) {
        why = failure.what();
        return nullptr;
    }
}

} // namespace opensu::retro
