// overlay_pass — draws the runner's own overlay (its touch controls, as an RGBA image the size of
// the target) over what the frame compositor drew, mixed by its alpha. Its render
// pass loads the target, so it runs after the compositor's on the same framebuffer.
#pragma once

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
    /// For targets of `format` that the compositor leaves in `layout`. Throws VulkanError.
    OverlayPass(VulkanDevice& device, VkFormat format, VkImageLayout layout);
    OverlayPass(const OverlayPass&) = delete;
    OverlayPass& operator=(const OverlayPass&) = delete;
    OverlayPass(OverlayPass&&) = delete;
    OverlayPass& operator=(OverlayPass&&) = delete;
    ~OverlayPass();

    /// Records uploading `overlay` when it changed or its size did; outside any render pass.
    void recordUpload(VkCommandBuffer commands, const touch::OverlayImage& overlay);
    /// Records drawing the uploaded overlay over `framebuffer` of `extent`, which the target's
    /// render pass made.
    void recordDraw(VkCommandBuffer commands, VkFramebuffer framebuffer, VkExtent2D extent) const;

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
