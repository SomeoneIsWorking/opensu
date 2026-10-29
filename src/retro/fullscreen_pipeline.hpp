// fullscreen_pipeline — the render passes and pipelines the runner draws a target with: one colour
// attachment and one triangle covering the viewport (shaders/frame.vert), its fragment shader the
// caller's, reading one sampled image.
#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

namespace opensu::retro {

/// The one image a fullscreen fragment shader samples (set 0, binding 0) and the sampler it is
/// read with.
class SampledInput {
  public:
    /// Throws VulkanError.
    SampledInput(VkDevice device, VkFilter filter);
    SampledInput(const SampledInput&) = delete;
    SampledInput& operator=(const SampledInput&) = delete;
    SampledInput(SampledInput&&) = delete;
    SampledInput& operator=(SampledInput&&) = delete;
    ~SampledInput();

    /// Points the descriptor at `view` in `layout`; the commands reading the last one must be done.
    void bind(VkImageView view, VkImageLayout layout);
    [[nodiscard]] VkImageView bound() const {
        return bound_;
    }
    [[nodiscard]] VkDescriptorSetLayout setLayout() const {
        return setLayout_;
    }
    [[nodiscard]] VkDescriptorSet set() const {
        return set_;
    }

  private:
    void release();

    VkDevice device_;
    VkDescriptorSetLayout setLayout_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkDescriptorPool pool_{VK_NULL_HANDLE};
    VkDescriptorSet set_{VK_NULL_HANDLE};
    VkImageView bound_{VK_NULL_HANDLE};
};

/// A render pass over one attachment of `format` entering in `initial` (UNDEFINED when `load` is
/// not LOAD) and left in `final`; its subpass waits for earlier drawing and transfers to the image,
/// and transfers after it wait for its writes. Throws VulkanError.
[[nodiscard]] VkRenderPass makeColourPass(VkDevice device, VkFormat format, VkAttachmentLoadOp load,
                                          VkImageLayout initial, VkImageLayout final);

/// A pipeline in `pass` drawing frame.vert's triangle with `fragment`, viewport and scissor set
/// when drawing; `blended` mixes it over the target by its alpha. Throws VulkanError.
[[nodiscard]] VkPipeline makeFullscreenPipeline(VkDevice device, VkRenderPass pass,
                                                VkPipelineLayout layout,
                                                std::span<const std::uint32_t> fragment,
                                                bool blended);

} // namespace opensu::retro
