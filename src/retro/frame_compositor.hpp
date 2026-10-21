// frame_compositor — what the runner draws into a target: the core's frame uploaded to a texture
// and drawn at the aspect, integer scale, sampling and filter of the game's video settings, the
// rest of the target black. With a shader preset, `shader_chain` draws the placed rectangle in
// place of the sampling and filter. The presenter owns the window, the targets and the submission;
// this owns the render pass every target's framebuffer is made for. `frame_filter` picks the
// built-in shader.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

#include <vulkan/vulkan.h>

#include "settings/video_settings.hpp"
#include "shader_chain.hpp"
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

/// Where `frame` is drawn in `extent` under `video`, centred. An overscaled frame is larger than
/// `extent` and starts before it.
[[nodiscard]] VkRect2D placeFrame(VkExtent2D extent, const FrameImage& frame,
                                  const settings::VideoSettings& video);

/// An image the compositor draws into, with the framebuffer made for its render pass.
struct DrawTarget {
    VkImage image{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
};

class FrameCompositor {
  public:
    /// Targets are images of `format`, left in `finalLayout`. A shader preset in `video` is built
    /// with the librashader at `librashader` on `instance`'s `device`. Throws VulkanError, or
    /// ShaderError when the preset cannot run.
    FrameCompositor(VkInstance instance, const VulkanDevice& device, VkFormat format,
                    VkImageLayout finalLayout, const settings::VideoSettings& video,
                    const std::filesystem::path& librashader);
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
    /// frame is whole. Reports whether it uploaded. Throws VulkanError or ShaderError.
    bool record(VkCommandBuffer commands, const FrameImage& frame, bool upload,
                const DrawTarget& target, VkExtent2D extent);

  private:
    void open(VkFormat format, VkImageLayout finalLayout);
    /// Everything `open` and the texture made, in reverse; safe when partly made.
    void release();
    void createRenderPass(VkFormat format, VkImageLayout finalLayout);
    void createPipeline();
    void ensureTexture(unsigned width, unsigned height);
    void destroyTexture();
    void recordUpload(VkCommandBuffer commands, const FrameImage& frame);
    void recordDraw(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                    const FrameImage& frame);
    /// The preset's passes into `target`, which ends in the render pass's final layout.
    void recordChain(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                     const FrameImage& frame);

    const VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    settings::VideoSettings video_;
    VkFormat format_{VK_FORMAT_UNDEFINED};
    VkImageLayout finalLayout_{VK_IMAGE_LAYOUT_UNDEFINED};
    /// The shader preset's chain; none without a preset.
    std::unique_ptr<ShaderChain> chain_;
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
