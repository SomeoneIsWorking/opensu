// gpu_spans — GPU time of recorded work by key (timestamps around a command buffer's work, read
// once its submission is done) and what each span dispatched, reported at run end in the
// perf-units line form.
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>

#include "lucent/count_histogram.h"

namespace opensu::frame_gen {

/// What a span dispatched, counted as it is recorded: compute dispatches, shader invocations and
/// the bytes of the images its dispatches read and write, each image once per dispatch.
struct GpuWork {
    std::uint64_t dispatches{0};
    std::uint64_t invocations{0};
    std::uint64_t bytesRead{0};
    std::uint64_t bytesWritten{0};
};

class GpuSpans {
  public:
    /// Timestamps on a queue whose family has `validBits` of them, `periodNs` apart; none when
    /// `validBits` is 0.
    GpuSpans(VkDevice device, float periodNs, std::uint32_t validBits);
    ~GpuSpans();
    GpuSpans(const GpuSpans&) = delete;
    GpuSpans& operator=(const GpuSpans&) = delete;
    GpuSpans(GpuSpans&&) = delete;
    GpuSpans& operator=(GpuSpans&&) = delete;

    /// The slot `end` takes, or nothing when every slot waits on its submission.
    [[nodiscard]] std::optional<std::uint32_t> begin(VkCommandBuffer cmd);
    void end(VkCommandBuffer cmd, std::uint32_t slot, const std::string& key);
    /// The submission carrying the span `end` closed last.
    void submitted(std::uint64_t seqNo);
    /// Reads the spans of submissions up to `completed`.
    void drain(std::uint64_t completed);
    /// What a span of `key` dispatched.
    void work(const std::string& key, const GpuWork& work);

    /// One line per key and unit: the work counts per span, then GPU time per span.
    void report() const;

  private:
    static constexpr std::uint32_t slots = 16;

    struct Pending {
        std::uint32_t slot{0};
        std::string key;
        std::uint64_t seqNo{0};
    };

    VkDevice device_;
    double periodNs_;
    std::uint64_t validMask_;
    VkQueryPool pool_{VK_NULL_HANDLE};
    std::uint32_t next_{0};
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
};

} // namespace opensu::frame_gen
