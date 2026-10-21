// frame_compositor — what the runner draws into a target: the core's frame uploaded to a texture
// and drawn with its aspect kept, letterboxed in black. The presenter owns the window, the targets
// and the submission; this owns the render pass every target's framebuffer is made for.
#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

#include "vulkan_device.hpp"

namespace opensu::retro {

/// A frame to show: rows of `width` RGBA pixels, top first, displayed `aspect` wide per unit high.
struct FrameImage {
    std::span<const std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
    float aspect{0.0F};
};

/// The largest rectangle of `aspect` centred in `extent`; all of it when `aspect` is not positive.
[[nodiscard]] VkRect2D letterbox(VkExtent2D extent, float aspect);

class FrameCompositor {
  public:
    /// Targets are images of `format`, left in `finalLayout`; throws VulkanError.
    FrameCompositor(const VulkanDevice& device, VkFormat format, VkImageLayout finalLayout);
    FrameCompositor(const FrameCompositor&) = delete;
    FrameCompositor& operator=(const FrameCompositor&) = delete;
    FrameCompositor(FrameCompositor&&) = delete;
    FrameCompositor& operator=(FrameCompositor&&) = delete;
    ~FrameCompositor();

    /// The render pass a target's framebuffer is made for.
    [[nodiscard]] VkRenderPass renderPass() const {
        return renderPass_;
    }

    /// Records drawing `frame` into `target` of `extent`, uploading it first when `upload` and the
    /// frame is whole. Reports whether it uploaded. Throws VulkanError.
    bool record(VkCommandBuffer commands, const FrameImage& frame, bool upload,
                VkFramebuffer target, VkExtent2D extent);

  private:
    void open(VkFormat format, VkImageLayout finalLayout);
    /// Everything `open` and the texture made, in reverse; safe when partly made.
    void release();
    void createRenderPass(VkFormat format, VkImageLayout finalLayout);
    void createPipeline();
    void ensureTexture(unsigned width, unsigned height);
    void destroyTexture();
    void recordUpload(VkCommandBuffer commands, const FrameImage& frame);
    void recordDraw(VkCommandBuffer commands, VkFramebuffer target, VkExtent2D extent,
                    float aspect);

    const VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkDescriptorSetLayout setLayout_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};

    VkImage texture_{VK_NULL_HANDLE};
    VkDeviceMemory textureMemory_{VK_NULL_HANDLE};
    VkImageView textureView_{VK_NULL_HANDLE};
    unsigned textureWidth_{0};
    unsigned textureHeight_{0};
    bool textureReady_{false};
    VkBuffer staging_{VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory_{VK_NULL_HANDLE};
    void* stagingMapped_{nullptr};
};

} // namespace opensu::retro
