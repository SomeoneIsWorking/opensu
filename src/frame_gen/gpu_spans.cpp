#include "frame_gen/gpu_spans.hpp"

#include <array>
#include <stdexcept>

#include "frame_gen/generator_gpu.hpp"
#include "lucent/log.h"

namespace opensu::frame_gen {

GpuSpans::GpuSpans(VkDevice device, float periodNs, std::uint32_t validBits, Measure measure)
    : device_{device}, periodNs_{periodNs},
      validMask_{validBits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << validBits) - 1},
      measure_{measure} {
    if (measure_ != Measure::time || validBits == 0) {
        return;
    }
    const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                     .queryCount = slots * stampsPerSpan,
                                     .pipelineStatistics = 0};
    check(vkCreateQueryPool(device_, &info, nullptr, &pool_), "vkCreateQueryPool");
}

GpuSpans::~GpuSpans() {
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device_, pool_, nullptr);
    }
}

std::optional<std::uint32_t> GpuSpans::begin(VkCommandBuffer cmd) {
    if (measure_ != Measure::time) {
        return std::nullopt;
    }
    if (pool_ == VK_NULL_HANDLE || pending_.size() >= slots) {
        ++dropped_;
        return std::nullopt;
    }
    const std::uint32_t slot = next_;
    next_ = (next_ + 1) % slots;
    marks_.at(slot).clear();
    vkCmdResetQueryPool(cmd, pool_, slot * stampsPerSpan, stampsPerSpan);
    // At the bottom of the pipe, after the work before it: a top-of-pipe stamp is written before
    // the span's first barrier waits on that work.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_, slot * stampsPerSpan);
    return slot;
}

void GpuSpans::mark(VkCommandBuffer cmd, std::uint32_t slot, const std::string& name) {
    std::vector<std::string>& marks = marks_.at(slot);
    if (marks.size() + 2 > stampsPerSpan) {
        throw std::logic_error{"more marks in a span than its timestamps"};
    }
    marks.push_back(name);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_,
                        (slot * stampsPerSpan) + static_cast<std::uint32_t>(marks.size()));
}

void GpuSpans::end(VkCommandBuffer cmd, std::uint32_t slot, const std::string& key) {
    std::vector<std::string>& marks = marks_.at(slot);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_,
                        (slot * stampsPerSpan) + static_cast<std::uint32_t>(marks.size()) + 1);
    pending_.push_back(Pending{.slot = slot, .key = key, .seqNo = 0, .marks = std::move(marks)});
    marks.clear();
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
        const auto count = static_cast<std::uint32_t>(span.marks.size()) + 2;
        std::array<std::uint64_t, stampsPerSpan> stamps{};
        const auto between = [&](std::size_t from, std::size_t to) {
            const std::uint64_t ticks = ((stamps.at(to) - stamps.at(from)) & validMask_);
            return static_cast<std::uint64_t>(static_cast<double>(ticks) * periodNs_);
        };
        if (vkGetQueryPoolResults(device_, pool_, span.slot * stampsPerSpan, count, sizeof(stamps),
                                  stamps.data(), sizeof(std::uint64_t),
                                  VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            nanoseconds_[span.key].add(between(0, count - 1));
            for (std::size_t i = 0; i < span.marks.size(); ++i) {
                nanoseconds_[span.key + " " + span.marks[i]].add(between(i, i + 1));
            }
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
    if (counting()) {
        Reads& reads = reads_[key];
        reads.texelReads.add(work.texelReads);
        reads.sharedReads.add(work.sharedReads);
    }
}

void perfLine(const std::string& name, const lucent::perf::CountHistogram& histogram,
              const std::string& unit, const std::string& per) {
    lucent::info("perf", "{}: p50 {} p99 {} max {} {}/{}, total {} {}", name,
                 histogram.percentile(0.5), histogram.percentile(0.99), histogram.max(), unit, per,
                 histogram.total(), unit);
}

void GpuSpans::report() const {
    std::uint64_t spans = 0;
    for (const auto& [key, histogram] : nanoseconds_) {
        spans += histogram.count();
    }
    lucent::info("perf", "framegen run-end measuring {}: {} spans timed, {} not, GPU ns per span",
                 counting() ? "counts" : "time", spans, dropped_);
    for (const auto& [key, counts] : work_) {
        perfLine(key + " dispatches", counts.dispatches, "dispatches", "span");
        perfLine(key + " invocations", counts.invocations, "invocations", "span");
        perfLine(key + " read", counts.bytesRead, "bytes", "span");
        perfLine(key + " written", counts.bytesWritten, "bytes", "span");
    }
    for (const auto& [key, counts] : reads_) {
        perfLine(key + " texel reads", counts.texelReads, "texels", "span");
        perfLine(key + " shared reads", counts.sharedReads, "values", "span");
    }
    for (const auto& [key, histogram] : nanoseconds_) {
        perfLine(key + " GPU time", histogram, "ns", "span");
    }
}

} // namespace opensu::frame_gen
