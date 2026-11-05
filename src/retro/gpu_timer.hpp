// gpu_timer — how long the GPU spent on a stretch of a command buffer: a timestamp written before
// it and one after, read once the frame's fence has passed. A queue without timestamps has no
// timer.
#pragma once

#include <chrono>
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

    /// Records the start, outside a render pass; the last stretch must have been taken or dropped.
    void recordStart(VkCommandBuffer commands);
    void recordEnd(VkCommandBuffer commands);

    /// The last recorded stretch, once its commands are done; nothing before, or twice.
    [[nodiscard]] std::optional<std::chrono::nanoseconds> take();

  private:
    VkDevice device_{VK_NULL_HANDLE};
    VkQueryPool pool_{VK_NULL_HANDLE};
    double nanosecondsPerTick_{1.0};
    /// The bits of a timestamp that count.
    std::uint64_t mask_{0};
    bool recorded_{false};
};

} // namespace opensu::retro
