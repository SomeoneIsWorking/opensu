// frame_compositor — what the runner draws into a target: the core's frame, uploaded to a texture
// or, from a Vulkan core, its own image read where it is, drawn at the aspect, integer scale,
// sampling and filter of the game's video settings, the rest of the target black. With a shader
// preset, `shader_chain` draws the placed rectangle from either in place of the sampling and
// filter, at the resolution `shader_scale` holds: below full, into `reduced_output`'s image, then
// drawn into the rectangle. The presenter owns the window, the targets and the submission; this
// owns the render pass every target's framebuffer is made for. `frame_filter` picks the built-in
// shader. It turns the picture a quarter for a rotated monitor (`turn`), a preset's by way of
// `reduced_output`, and keeps it off a band along the bottom (`reserve`).
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

#include <vulkan/vulkan.h>

#include "frame_stages.hpp"
#include "fullscreen_pipeline.hpp"
#include "gpu_timer.hpp"
#include "reduced_output.hpp"
#include "settings/video_settings.hpp"
#include "shader_chain.hpp"
#include "shader_scale.hpp"
#include "streamed_texture.hpp"
#include "vulkan_device.hpp"

namespace opensu::retro {

/// A Vulkan core's image of a frame (set_image), read in the layout the core left it in.
struct CoreImage {
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    /// The queue family that owns it; VK_QUEUE_FAMILY_IGNORED or the runner's own for no transfer.
    std::uint32_t queueFamily{VK_QUEUE_FAMILY_IGNORED};
};

/// A frame to show, `width` by `height` and displayed `aspect` wide per unit high: rows of RGBA
/// pixels, top first, or a Vulkan core's image in `gpu`. It is on screen for `interval`, the
/// GPU's budget for drawing it.
struct FrameImage {
    std::span<const std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
    float aspect{0.0F};
    std::optional<CoreImage> gpu;
    std::chrono::nanoseconds interval{};
};

/// The largest rectangle of `aspect` centred in `extent`; all of it when `aspect` is not positive.
[[nodiscard]] VkRect2D letterbox(VkExtent2D extent, float aspect);

/// Where `frame` is drawn in `extent` under `video`, centred, `turned` a quarter. An overscaled
/// frame is larger than `extent` and starts before it.
[[nodiscard]] VkRect2D placeFrame(VkExtent2D extent, const FrameImage& frame,
                                  const settings::VideoSettings& video, bool turned = false);

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
    FrameCompositor(VkInstance instance, VulkanDevice& device, VkFormat format,
                    VkImageLayout finalLayout, const settings::VideoSettings& video,
                    const std::filesystem::path& librashader);
    FrameCompositor(const FrameCompositor&) = delete;
    FrameCompositor& operator=(const FrameCompositor&) = delete;
    FrameCompositor(FrameCompositor&&) = delete;
    FrameCompositor& operator=(FrameCompositor&&) = delete;
    ~FrameCompositor();

    /// Shows frames under `video` from the next one drawn: the pipeline and sampler made again,
    /// and a different shader preset built on `instance` first, so one that cannot run throws
    /// ShaderError and leaves the settings in force. Waits for the GPU. Throws VulkanError.
    void change(const settings::VideoSettings& video, VkInstance instance,
                const std::filesystem::path& librashader);

    /// The settings in force.
    [[nodiscard]] const settings::VideoSettings& video() const {
        return video_;
    }

    /// Draws the picture a quarter turn clockwise from the next frame, or upright again. Waits for
    /// the GPU. Throws VulkanError.
    void turn(bool turned);
    /// Keeps the picture off the target's top `top` pixels (a screen's cutout) and bottom `bottom`
    /// (the touch controls' band).
    void reserve(std::uint32_t top, std::uint32_t bottom) noexcept {
        clearTop_ = top;
        clearBottom_ = bottom;
    }

    /// Where `frame` is drawn in `extent` under the settings in force.
    [[nodiscard]] VkRect2D placed(VkExtent2D extent, const FrameImage& frame) const {
        const std::uint32_t top = std::min(clearTop_, extent.height);
        const std::uint32_t height =
            extent.height - top - std::min(clearBottom_, extent.height - top);
        VkRect2D rect = placeFrame({extent.width, height}, frame, video_, turned_);
        rect.offset.y += static_cast<std::int32_t>(top);
        return rect;
    }

    /// The render pass a target's framebuffer is made for.
    [[nodiscard]] VkRenderPass renderPass() const {
        return renderPass_;
    }

    /// Records drawing `frame` into `target` of `extent`, uploading it first when `upload` and the
    /// frame is whole. Reports whether it uploaded. A core's image is read where it is, and the
    /// core's next writes to it wait for that read. The GPU time of the last frame's shader preset
    /// goes to `stages`. Throws VulkanError or ShaderError.
    bool record(VkCommandBuffer commands, const FrameImage& frame, bool upload,
                const DrawTarget& target, VkExtent2D extent, FrameStages& stages);

  private:
    /// Everything the constructor made, in reverse; safe when partly made.
    void release();
    /// The sampled input, its pipeline layout and the pipeline, for `video_`.
    void createPipeline();
    /// What `createPipeline` made, in reverse; safe when partly made.
    void destroyPipeline();
    /// Points the input at the frame texture unless it already is; the previous frame's commands
    /// must be done.
    void bindTexture();
    void recordUpload(VkCommandBuffer commands, const FrameImage& frame);
    /// The core's image into the frame texture, converted; for a preset when the core keeps the
    /// image in GENERAL, which the runner may not change.
    void recordCopy(VkCommandBuffer commands, const FrameImage& frame, const CoreImage& image);
    void recordCoreImage(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                         const FrameImage& frame, const CoreImage& image);
    /// The frame from `source` (the bound input, or the preset's input).
    void recordDraw(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                    const FrameImage& frame, const ChainImage& source, bool ready);
    /// The preset's passes from `source` into `target`, which ends in the render pass's final
    /// layout, at the scale `scale_` holds, timed.
    void recordChain(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                     const FrameImage& frame, const ChainImage& source);
    /// The passes drawn straight into `rect` of `target`.
    void recordFullChain(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                         const FrameImage& frame, const ChainImage& source, VkRect2D rect);
    /// The passes drawn into the reduced output at `scale_`, then it into `rect` of `target`.
    void recordReducedChain(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent,
                            const FrameImage& frame, const ChainImage& source, VkRect2D rect);
    [[nodiscard]] Facing facing() const noexcept {
        return turned_ ? Facing::Turned : Facing::Upright;
    }
    /// Begins the render pass on `target`, cleared black.
    void beginPass(VkCommandBuffer commands, const DrawTarget& target, VkExtent2D extent) const;
    /// Under Auto, says once when the GPU cannot be timed, so the scale stays full.
    void noteUntimed() const;
    /// The last frame's shader time to `stages` and `scale_`, once its commands are done.
    void takeShaderTime(const FrameImage& frame, FrameStages& stages);
    [[nodiscard]] ChainImage textureImage() const;

    VulkanDevice& device_;
    VkDevice vk_{VK_NULL_HANDLE};
    settings::VideoSettings video_;
    VkFormat format_{VK_FORMAT_UNDEFINED};
    VkImageLayout finalLayout_{VK_IMAGE_LAYOUT_UNDEFINED};
    /// The shader preset's chain; none without a preset.
    std::unique_ptr<ShaderChain> chain_;
    ShaderScale scale_;
    GpuTimer timer_;
    /// Made when the chain is first drawn below full resolution.
    std::unique_ptr<ReducedOutput> reduced_;
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    /// Made again with the pipeline: its sampler follows the video settings.
    std::unique_ptr<SampledInput> input_;
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    StreamedTexture texture_;
    bool turned_{false};
    std::uint32_t clearTop_{0};
    std::uint32_t clearBottom_{0};
};

} // namespace opensu::retro
