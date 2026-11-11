// fullscreen_pipeline — the render passes and pipelines the runner draws a target with: one colour
// attachment and one triangle covering the viewport (shaders/frame.vert), its fragment shader the
// caller's, reading one sampled image.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

namespace opensu::retro {

/// The one image a fullscreen fragment shader samples (set 0, binding 0) and the sampler it is
/// read with, in one descriptor set per frame slot, so a slot's set can be pointed elsewhere while
/// another slot's frame still reads its own.
class SampledInput {
  public:
    /// `slots` descriptor sets. Throws VulkanError.
    SampledInput(VkDevice device, VkFilter filter, std::size_t slots = 1);
    SampledInput(const SampledInput&) = delete;
    SampledInput& operator=(const SampledInput&) = delete;
    SampledInput(SampledInput&&) = delete;
    SampledInput& operator=(SampledInput&&) = delete;
    ~SampledInput();

    /// Points every slot's set at `view` in `layout`; no commands reading any may be pending.
    void bind(VkImageView view, VkImageLayout layout);
    /// Points `slot`'s set at `view` in `layout`; no commands reading it may be pending.
    void bind(std::size_t slot, VkImageView view, VkImageLayout layout);
    [[nodiscard]] VkDescriptorSetLayout setLayout() const {
        return setLayout_;
    }
    [[nodiscard]] VkDescriptorSet set(std::size_t slot = 0) const {
        return sets_.at(slot);
    }

  private:
    void release();

    VkDevice device_;
    VkDescriptorSetLayout setLayout_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkDescriptorPool pool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> sets_;
};

/// A render pass over one attachment of `format` entering in `initial` (UNDEFINED when `load` is
/// not LOAD) and left in `final`; its subpass waits for earlier drawing and transfers to the image,
/// and transfers after it wait for its writes. Throws VulkanError.
[[nodiscard]] VkRenderPass makeColourPass(VkDevice device, VkFormat format, VkAttachmentLoadOp load,
                                          VkImageLayout initial, VkImageLayout final);

/// How a pipeline faces what it draws in its viewport.
enum class Facing : std::uint8_t {
    Upright,
    /// A quarter turn clockwise.
    Turned,
};

/// A pipeline in `pass` drawing frame.vert's triangle with `fragment`, viewport and scissor set
/// when drawing; `blended` mixes it over the target by its alpha. Throws VulkanError.
[[nodiscard]] VkPipeline makeFullscreenPipeline(VkDevice device, VkRenderPass pass,
                                                VkPipelineLayout layout,
                                                std::span<const std::uint32_t> fragment,
                                                bool blended, Facing facing = Facing::Upright);

} // namespace opensu::retro
