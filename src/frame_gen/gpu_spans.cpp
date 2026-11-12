#include "frame_gen/gpu_spans.hpp"

#include <array>

#include "frame_gen/generator_gpu.hpp"
#include "lucent/log.h"

namespace opensu::frame_gen {

GpuSpans::GpuSpans(VkDevice device, float periodNs, std::uint32_t validBits)
    : device_{device}, periodNs_{periodNs},
      validMask_{validBits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << validBits) - 1} {
    if (validBits == 0) {
        return;
    }
    const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                     .queryCount = slots * 2,
                                     .pipelineStatistics = 0};
    check(vkCreateQueryPool(device_, &info, nullptr, &pool_), "vkCreateQueryPool");
}

GpuSpans::~GpuSpans() {
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device_, pool_, nullptr);
    }
}

std::optional<std::uint32_t> GpuSpans::begin(VkCommandBuffer cmd) {
    if (pool_ == VK_NULL_HANDLE || pending_.size() >= slots) {
        ++dropped_;
        return std::nullopt;
    }
    const std::uint32_t slot = next_;
    next_ = (next_ + 1) % slots;
    vkCmdResetQueryPool(cmd, pool_, slot * 2, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool_, slot * 2);
    return slot;
}

void GpuSpans::end(VkCommandBuffer cmd, std::uint32_t slot, const std::string& key) {
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_, (slot * 2) + 1);
    pending_.push_back(Pending{.slot = slot, .key = key, .seqNo = 0});
}

void GpuSpans::submitted(std::uint64_t seqNo) {
    if (!pending_.empty()) {
        pending_.back().seqNo = seqNo;
    }
}

void GpuSpans::drain(std::uint64_t completed) {
    while (!pending_.empty() && pending_.front().seqNo != 0 &&
           pending_.front().seqNo <= completed) {
        const Pending& span = pending_.front();
        std::array<std::uint64_t, 2> stamps{};
        if (vkGetQueryPoolResults(device_, pool_, span.slot * 2, 2, sizeof(stamps), stamps.data(),
                                  sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            const std::uint64_t ticks = ((stamps[1] - stamps[0]) & validMask_);
            nanoseconds_[span.key].add(
                static_cast<std::uint64_t>(static_cast<double>(ticks) * periodNs_));
        } else {
            ++dropped_;
        }
        pending_.pop_front();
    }
}

void GpuSpans::work(const std::string& key, const GpuWork& work) {
    Counts& counts = work_[key];
    counts.dispatches.add(work.dispatches);
    counts.invocations.add(work.invocations);
    counts.bytesRead.add(work.bytesRead);
    counts.bytesWritten.add(work.bytesWritten);
    counts.texelReads.add(work.texelReads);
}

namespace {

void line(const std::string& name, const lucent::perf::CountHistogram& histogram,
          const char* unit) {
    lucent::info("perf", "{}: p50 {} p99 {} max {} {}/span, total {} {}", name,
                 histogram.percentile(0.5), histogram.percentile(0.99), histogram.max(), unit,
                 histogram.total(), unit);
}

} // namespace

void GpuSpans::report() const {
    std::uint64_t spans = 0;
    for (const auto& [key, histogram] : nanoseconds_) {
        spans += histogram.count();
    }
    lucent::info("perf", "framegen run-end: {} spans timed, {} not, GPU ns per span", spans,
                 dropped_);
    for (const auto& [key, counts] : work_) {
        line(key + " dispatches", counts.dispatches, "dispatches");
        line(key + " invocations", counts.invocations, "invocations");
        line(key + " read", counts.bytesRead, "bytes");
        line(key + " written", counts.bytesWritten, "bytes");
        line(key + " texel reads", counts.texelReads, "texels");
    }
    for (const auto& [key, histogram] : nanoseconds_) {
        line(key + " GPU time", histogram, "ns");
    }
}

} // namespace opensu::frame_gen
