// gpu_timer — how long the GPU spent on a stretch of a command buffer: a timestamp written before
// it and one after, a pair per frame slot, read once that slot's fence has passed. A queue without
// timestamps has no timer.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <vulkan/vulkan.h>

#include "vulkan_device.hpp"

namespace opensu::retro {

class GpuTimer {
  public:
    /// Throws VulkanError.
    explicit GpuTimer(const VulkanDevice& device);
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    GpuTimer(GpuTimer&&) = delete;
    GpuTimer& operator=(GpuTimer&&) = delete;
    ~GpuTimer();

    /// Whether the device's queue writes timestamps.
    [[nodiscard]] bool available() const {
        return pool_ != VK_NULL_HANDLE;
    }

    /// Records the start of `slot`'s stretch, outside a render pass; that slot's last stretch must
    /// have been taken or dropped.
    void recordStart(VkCommandBuffer commands, std::size_t slot);
    void recordEnd(VkCommandBuffer commands, std::size_t slot);

    /// `slot`'s last recorded stretch, once its commands are done; nothing before, or twice.
    [[nodiscard]] std::optional<std::chrono::nanoseconds> take(std::size_t slot);

  private:
    /// `slot`'s start query; its end is the next.
    [[nodiscard]] static std::uint32_t firstQuery(std::size_t slot) {
        return static_cast<std::uint32_t>(2 * slot);
    }

    VkDevice device_{VK_NULL_HANDLE};
    VkQueryPool pool_{VK_NULL_HANDLE};
    double nanosecondsPerTick_{1.0};
    /// The bits of a timestamp that count.
    std::uint64_t mask_{0};
    std::array<bool, framesInFlight> recorded_{};
};

} // namespace opensu::retro
