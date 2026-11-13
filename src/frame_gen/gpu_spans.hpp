// gpu_spans — what each span of recorded work dispatched and, in a run that times it, its GPU
// time and each dispatch's (timestamps read once its submission is done), reported at run end in
// the perf-units line form.
#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lucent/count_histogram.h"

namespace opensu::frame_gen {

/// What a measuring run records: the reads its shaders count, which slows them, or the GPU time
/// of the shaders that ship.
enum class Measure : std::uint8_t { counts, time };

/// What a span dispatched, counted as it is recorded: compute dispatches, shader invocations and
/// the bytes of the images its dispatches read and write, each image once per dispatch; and the
/// texels its shaders read from images and the values they read from shared memory, counted by
/// the shaders.
struct GpuWork {
    std::uint64_t dispatches{0};
    std::uint64_t invocations{0};
    std::uint64_t bytesRead{0};
    std::uint64_t bytesWritten{0};
    std::uint64_t texelReads{0};
    std::uint64_t sharedReads{0};
};

/// Logs `histogram` as one `[perf]` line: `name`'s p50, p99 and max in `unit` per `per`, and the
/// run's total (the form `perf_compare.py` reads).
void perfLine(const std::string& name, const lucent::perf::CountHistogram& histogram,
              const std::string& unit, const std::string& per);

class GpuSpans {
  public:
    /// Timestamps, when `measure` is time, on a queue whose family has `validBits` of them,
    /// `periodNs` apart; none when `validBits` is 0.
    GpuSpans(VkDevice device, float periodNs, std::uint32_t validBits, Measure measure);
    ~GpuSpans();
    GpuSpans(const GpuSpans&) = delete;
    GpuSpans& operator=(const GpuSpans&) = delete;
    GpuSpans(GpuSpans&&) = delete;
    GpuSpans& operator=(GpuSpans&&) = delete;

    /// Whether the shaders count their reads.
    [[nodiscard]] bool counting() const noexcept {
        return measure_ == Measure::counts;
    }

    /// The slot `mark` and `end` take, or nothing when not timing or every slot waits on its
    /// submission. The span starts once the work submitted before it is done.
    [[nodiscard]] std::optional<std::uint32_t> begin(VkCommandBuffer cmd);
    /// Ends the span's part `name`, which began at the span's start or its last mark.
    void mark(VkCommandBuffer cmd, std::uint32_t slot, const std::string& name);
    void end(VkCommandBuffer cmd, std::uint32_t slot, const std::string& key);
    /// The submission carrying the span `end` closed last.
    void submitted(std::uint64_t seqNo);
    /// Reads the spans of submissions up to `completed`.
    void drain(std::uint64_t completed);
    /// What a span of `key` dispatched.
    void work(const std::string& key, const GpuWork& work);

    /// One line per key and unit: the work counts per span, then GPU time per span and part.
    void report() const;

  private:
    static constexpr std::uint32_t slots = 16;
    /// A span's start, its marks and its end.
    static constexpr std::uint32_t stampsPerSpan = 16;

    struct Pending {
        std::uint32_t slot{0};
        std::string key;
        std::uint64_t seqNo{0};
        std::vector<std::string> marks;
    };

    VkDevice device_;
    double periodNs_;
    std::uint64_t validMask_;
    Measure measure_;
    VkQueryPool pool_{VK_NULL_HANDLE};
    std::uint32_t next_{0};
    /// The marks of each slot's span not yet ended.
    std::array<std::vector<std::string>, slots> marks_;
    std::deque<Pending> pending_;
    std::uint64_t dropped_{0};
    std::map<std::string, lucent::perf::CountHistogram> nanoseconds_;
    struct Counts {
        lucent::perf::CountHistogram dispatches;
        lucent::perf::CountHistogram invocations;
        lucent::perf::CountHistogram bytesRead;
        lucent::perf::CountHistogram bytesWritten;
    };
    std::map<std::string, Counts> work_;
    /// Only while the shaders count them.
    struct Reads {
        lucent::perf::CountHistogram texelReads;
        lucent::perf::CountHistogram sharedReads;
    };
    std::map<std::string, Reads> reads_;
};

} // namespace opensu::frame_gen
