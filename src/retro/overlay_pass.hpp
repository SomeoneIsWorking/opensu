// overlay_pass — draws an RGBA image (straight alpha) into a rectangle of the target over what is
// there, mixed by its alpha: the runner's touch controls over the whole target, and a visual
// preset's frame art around the picture. Its render pass loads the target, so it runs after the
// frame compositor's on the same framebuffer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

#include "fullscreen_pipeline.hpp"
#include "streamed_texture.hpp"
#include "touch/touch_surface.hpp"
#include "vulkan_device.hpp"

namespace opensu::retro {

class OverlayPass {
  public:
    /// For targets of `format` that the compositor leaves in `layout`, the image sampled with
    /// `filter`. Throws VulkanError.
    OverlayPass(VulkanDevice& device, VkFormat format, VkImageLayout layout, VkFilter filter);
    OverlayPass(const OverlayPass&) = delete;
    OverlayPass& operator=(const OverlayPass&) = delete;
    OverlayPass(OverlayPass&&) = delete;
    OverlayPass& operator=(OverlayPass&&) = delete;
    ~OverlayPass();

    /// Records uploading `overlay` through frame slot `slot` when it changed or its size did;
    /// outside any render pass.
    void recordUpload(VkCommandBuffer commands, const touch::OverlayImage& overlay,
                      std::size_t slot);
    /// Records drawing the uploaded overlay into `destination` of `framebuffer` of `extent`,
    /// which the target's render pass made.
    void recordDraw(VkCommandBuffer commands, VkFramebuffer framebuffer, VkExtent2D extent,
                    VkRect2D destination) const;

  private:
    void release();

    VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    StreamedTexture texture_;
    SampledInput input_;
    VkRenderPass pass_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

} // namespace opensu::retro
