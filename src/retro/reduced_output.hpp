// reduced_output — where a shader preset's chain draws below full resolution: an image of the
// reduced size, which the chain treats as its whole viewport, then drawn bilinearly into the placed
// rectangle of the target inside the frame compositor's render pass.
#pragma once

#include <memory>

#include <vulkan/vulkan.h>

#include "fullscreen_pipeline.hpp"
#include "shader_chain.hpp"
#include "vulkan_device.hpp"

namespace opensu::retro {

class ReducedOutput {
  public:
    /// Images of `format`, drawn into targets through `pass` facing `facing`. Throws VulkanError.
    ReducedOutput(VulkanDevice& device, VkFormat format, VkRenderPass pass, Facing facing);
    ReducedOutput(const ReducedOutput&) = delete;
    ReducedOutput& operator=(const ReducedOutput&) = delete;
    ReducedOutput(ReducedOutput&&) = delete;
    ReducedOutput& operator=(ReducedOutput&&) = delete;
    ~ReducedOutput();

    /// Records making the image `extent` (waiting for the GPU when its size changes) ready for the
    /// chain to draw into, and returns it. Throws VulkanError.
    [[nodiscard]] ChainImage recordBegin(VkCommandBuffer commands, VkExtent2D extent);
    /// Records making what the chain drew readable; outside a render pass.
    void recordEnd(VkCommandBuffer commands) const;
    /// Records drawing it over the viewport set inside the target's render pass.
    void recordDraw(VkCommandBuffer commands) const;

  private:
    void destroyImage();
    void release();

    VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    std::unique_ptr<SampledInput> input_;
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
    VkExtent2D extent_{};
};

} // namespace opensu::retro
