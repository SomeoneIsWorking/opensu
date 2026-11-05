#include "gpu_timer.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace opensu::retro {

GpuTimer::GpuTimer(const VulkanDevice& device) : device_{device.handle()} {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical(), &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical(), &count, families.data());
    const std::uint32_t bits =
        device.queueFamily() < count ? families[device.queueFamily()].timestampValidBits : 0;
    if (bits == 0) {
        return;
    }
    mask_ = bits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device.physical(), &properties);
    nanosecondsPerTick_ = properties.limits.timestampPeriod;
    VkQueryPoolCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create.queryType = VK_QUERY_TYPE_TIMESTAMP;
    create.queryCount = 2;
    check(vkCreateQueryPool(device_, &create, nullptr, &pool_), "vkCreateQueryPool");
}

GpuTimer::~GpuTimer() {
    vkDestroyQueryPool(device_, pool_, nullptr);
}

void GpuTimer::recordStart(VkCommandBuffer commands) {
    if (!available()) {
        return;
    }
    vkCmdResetQueryPool(commands, pool_, 0, 2);
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool_, 0);
}

void GpuTimer::recordEnd(VkCommandBuffer commands) {
    if (!available()) {
        return;
    }
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_, 1);
    recorded_ = true;
}

std::optional<std::chrono::nanoseconds> GpuTimer::take() {
    if (!recorded_) {
        return std::nullopt;
    }
    std::array<std::uint64_t, 2> ticks{};
    const VkResult result = vkGetQueryPoolResults(device_, pool_, 0, 2, sizeof(ticks), ticks.data(),
                                                  sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result != VK_SUCCESS) {
        return std::nullopt;
    }
    recorded_ = false;
    const std::uint64_t spent = ((ticks[1] & mask_) - (ticks[0] & mask_)) & mask_;
    return std::chrono::nanoseconds{
        static_cast<std::int64_t>(static_cast<double>(spent) * nanosecondsPerTick_)};
}

} // namespace opensu::retro
