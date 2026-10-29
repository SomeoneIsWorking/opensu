#include "frame_compositor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "frame_filter.hpp"
#include "fullscreen_pipeline.hpp"

namespace opensu::retro {
namespace {

/// The whole multiple of `size` that fits `room`, or with `overscale` the one that fills it; at
/// least 1.
float wholeScale(float room, float size, bool overscale) {
    const float fit = room / size;
    return std::max(1.0F, overscale ? std::ceil(fit) : std::floor(fit));
}

/// The part of `rect` inside `extent`.
VkRect2D clipped(VkRect2D rect, VkExtent2D extent) {
    const auto left = std::max<std::int64_t>(rect.offset.x, 0);
    const auto top = std::max<std::int64_t>(rect.offset.y, 0);
    const auto right =
        std::min<std::int64_t>(rect.offset.x + std::int64_t{rect.extent.width}, extent.width);
    const auto bottom =
        std::min<std::int64_t>(rect.offset.y + std::int64_t{rect.extent.height}, extent.height);
    VkRect2D inside{};
    inside.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    inside.extent = {static_cast<std::uint32_t>(std::max<std::int64_t>(right - left, 0)),
                     static_cast<std::uint32_t>(std::max<std::int64_t>(bottom - top, 0))};
    return inside;
}

} // namespace

VkRect2D letterbox(VkExtent2D extent, float aspect) {
    const float width = static_cast<float>(extent.width);
    const float height = static_cast<float>(extent.height);
    float drawnWidth = width;
    float drawnHeight = height;
    if (aspect > 0.0F) {
        if (width / height > aspect) {
            drawnWidth = height * aspect;
        } else {
            drawnHeight = width / aspect;
        }
    }
    VkRect2D rect{};
    rect.extent.width = std::max(1U, static_cast<std::uint32_t>(std::lround(drawnWidth)));
    rect.extent.height = std::max(1U, static_cast<std::uint32_t>(std::lround(drawnHeight)));
    rect.offset.x = static_cast<std::int32_t>((extent.width - rect.extent.width) / 2);
    rect.offset.y = static_cast<std::int32_t>((extent.height - rect.extent.height) / 2);
    return rect;
}

VkRect2D placeFrame(VkExtent2D extent, const FrameImage& frame,
                    const settings::VideoSettings& video) {
    using settings::Aspect;
    using settings::IntegerScale;
    const auto width = static_cast<float>(frame.width);
    const auto height = static_cast<float>(frame.height);
    const auto roomWidth = static_cast<float>(extent.width);
    const auto roomHeight = static_cast<float>(extent.height);
    const float square = frame.height == 0 ? 0.0F : width / height;
    float aspect = 0.0F;
    switch (video.aspect) {
    case Aspect::Core:
        aspect = frame.aspect > 0.0F ? frame.aspect : square;
        break;
    case Aspect::FourThree:
        aspect = 4.0F / 3.0F;
        break;
    case Aspect::Square:
        aspect = square;
        break;
    case Aspect::Stretch:
        break;
    }
    if (video.integer == IntegerScale::Off || frame.width == 0 || frame.height == 0) {
        return letterbox(extent, aspect);
    }
    const bool overscale = video.integer == IntegerScale::Overscale;
    float drawnWidth = 0.0F;
    float drawnHeight = 0.0F;
    if (video.aspect == Aspect::Stretch) {
        drawnWidth = width * wholeScale(roomWidth, width, overscale);
        drawnHeight = height * wholeScale(roomHeight, height, overscale);
    } else {
        // Whole multiples of the frame's height, its width following at the aspect.
        const float scale = wholeScale(std::min(roomHeight, roomWidth / aspect), height, overscale);
        drawnHeight = height * scale;
        drawnWidth = video.aspect == Aspect::Square ? width * scale : drawnHeight * aspect;
    }
    VkRect2D rect{};
    rect.extent.width = std::max(1U, static_cast<std::uint32_t>(std::lround(drawnWidth)));
    rect.extent.height = std::max(1U, static_cast<std::uint32_t>(std::lround(drawnHeight)));
    rect.offset.x = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(extent.width) - rect.extent.width) / 2);
    rect.offset.y = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(extent.height) - rect.extent.height) / 2);
    return rect;
}

FrameCompositor::FrameCompositor(VkInstance instance, VulkanDevice& device, VkFormat format,
                                 VkImageLayout finalLayout, const settings::VideoSettings& video,
                                 const std::filesystem::path& librashader)
    : device_{device}, vk_{device.handle()}, video_{video}, format_{format},
      finalLayout_{finalLayout},
      // librashader copies the frame into a preset's OriginalHistory.
      texture_{device, VK_IMAGE_USAGE_TRANSFER_SRC_BIT} {
    try {
        renderPass_ = makeColourPass(vk_, format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_IMAGE_LAYOUT_UNDEFINED, finalLayout);
        createPipeline();
        if (!video_.shaderPreset.empty()) {
            chain_ =
                std::make_unique<ShaderChain>(librashader, video_.shaderPreset, instance, device_);
        }
    } catch (...) {
        release();
        throw;
    }
}

FrameCompositor::~FrameCompositor() {
    release();
}

void FrameCompositor::release() {
    device_.waitIdle();
    chain_.reset();
    destroyPipeline();
    vkDestroyRenderPass(vk_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
}

void FrameCompositor::destroyPipeline() {
    vkDestroyPipeline(vk_, pipeline_, nullptr);
    vkDestroyPipelineLayout(vk_, pipelineLayout_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    input_.reset();
}

void FrameCompositor::change(const settings::VideoSettings& video, VkInstance instance,
                             const std::filesystem::path& librashader) {
    std::unique_ptr<ShaderChain> chain;
    const bool samePreset = video.shaderPreset == video_.shaderPreset;
    if (!video.shaderPreset.empty() && !samePreset) {
        chain = std::make_unique<ShaderChain>(librashader, video.shaderPreset, instance, device_);
    }
    check(device_.waitIdle(), "vkDeviceWaitIdle");
    destroyPipeline();
    video_ = video;
    if (!samePreset) {
        chain_ = std::move(chain);
    }
    createPipeline();
}

void FrameCompositor::createPipeline() {
    input_ = std::make_unique<SampledInput>(
        vk_, video_.sampling == settings::Sampling::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
    const VkDescriptorSetLayout setLayout = input_->setLayout();
    const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, framePushSize};
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &setLayout;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(vk_, &layout, nullptr, &pipelineLayout_),
          "vkCreatePipelineLayout");
    pipeline_ =
        makeFullscreenPipeline(vk_, renderPass_, pipelineLayout_, fragmentShader(video_), false);
    if (texture_.ready()) {
        input_->bind(texture_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void FrameCompositor::bindTexture() {
    if (input_->bound() != texture_.view()) {
        input_->bind(texture_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void FrameCompositor::recordUpload(VkCommandBuffer commands, const FrameImage& frame) {
    texture_.ensure(frame.width, frame.height);
    texture_.recordUpload(commands, frame.rgba);
    bindTexture();
}

void FrameCompositor::recordChain(VkCommandBuffer commands, const DrawTarget& target,
                                  VkExtent2D extent, const FrameImage& frame,
                                  const ChainImage& source) {
    // The chain clears the whole target itself, so what it held before is not kept.
    imageBarrier(commands, target.image, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    chain_->record(commands, source, ChainImage{target.image, format_, extent},
                   placeFrame(extent, frame, video_), frame.aspect);
    const bool readBack = finalLayout_ == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier(commands, target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, finalLayout_,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                 readBack ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                 readBack ? VK_ACCESS_TRANSFER_READ_BIT : 0);
}

void FrameCompositor::recordDraw(VkCommandBuffer commands, const DrawTarget& target,
                                 VkExtent2D extent, const FrameImage& frame,
                                 const ChainImage& source, bool ready) {
    if (chain_ && ready) {
        recordChain(commands, target, extent, frame, source);
        return;
    }
    VkClearValue black{};
    black.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = renderPass_;
    begin.framebuffer = target.framebuffer;
    begin.renderArea.extent = extent;
    begin.clearValueCount = 1;
    begin.pClearValues = &black;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    const VkRect2D rect = placeFrame(extent, frame, video_);
    const VkRect2D scissor = clipped(rect, extent);
    if (ready && scissor.extent.width > 0 && scissor.extent.height > 0) {
        VkViewport viewport{};
        viewport.x = static_cast<float>(rect.offset.x);
        viewport.y = static_cast<float>(rect.offset.y);
        viewport.width = static_cast<float>(rect.extent.width);
        viewport.height = static_cast<float>(rect.extent.height);
        viewport.maxDepth = 1.0F;
        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        const VkDescriptorSet set = input_->set();
        vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                                &set, 0, nullptr);
        vkCmdSetViewport(commands, 0, 1, &viewport);
        vkCmdSetScissor(commands, 0, 1, &scissor);
        const auto texelsWide = static_cast<float>(source.extent.width);
        const auto texelsHigh = static_cast<float>(source.extent.height);
        const FramePush push = framePush(video_, {texelsWide, texelsHigh},
                                         {wholeScale(viewport.width, texelsWide, false),
                                          wholeScale(viewport.height, texelsHigh, false)});
        vkCmdPushConstants(commands, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, push.size,
                           push.bytes.data());
        vkCmdDraw(commands, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commands);
}

ChainImage FrameCompositor::textureImage() const {
    return ChainImage{
        texture_.image(), VK_FORMAT_R8G8B8A8_UNORM, {texture_.width(), texture_.height()}};
}

void FrameCompositor::recordCopy(VkCommandBuffer commands, const FrameImage& frame,
                                 const CoreImage& image) {
    texture_.ensure(frame.width, frame.height);
    texture_.beginWrite(commands);
    const auto wide = static_cast<std::int32_t>(frame.width);
    const auto high = static_cast<std::int32_t>(frame.height);
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {wide, high, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[1] = {wide, high, 1};
    vkCmdBlitImage(commands, image.image, image.layout, texture_.image(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
    texture_.endWrite(commands);
}

void FrameCompositor::recordCoreImage(VkCommandBuffer commands, const DrawTarget& target,
                                      VkExtent2D extent, const FrameImage& frame,
                                      const CoreImage& image) {
    const std::uint32_t own = device_.queueFamily();
    const bool transfer = image.queueFamily != VK_QUEUE_FAMILY_IGNORED && image.queueFamily != own;
    VkImageMemoryBarrier ownership{};
    ownership.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ownership.oldLayout = image.layout;
    ownership.newLayout = image.layout;
    ownership.image = image.image;
    ownership.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                  VK_REMAINING_ARRAY_LAYERS};
    if (transfer) {
        ownership.srcQueueFamilyIndex = image.queueFamily;
        ownership.dstQueueFamilyIndex = own;
        ownership.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &ownership);
    }
    const VkExtent2D size{frame.width, frame.height};
    if (!chain_) {
        // Every frame: a view the core destroyed and made again can come back with the same handle.
        input_->bind(image.view, image.layout);
        recordDraw(commands, target, extent, frame, ChainImage{image.image, image.format, size},
                   true);
    } else if (image.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // librashader reads it there and copies it for the preset's history (TRANSFER_SRC).
        recordDraw(commands, target, extent, frame, ChainImage{image.image, image.format, size},
                   true);
    } else {
        recordCopy(commands, frame, image);
        recordDraw(commands, target, extent, frame, textureImage(), true);
    }
    if (transfer) {
        ownership.srcQueueFamilyIndex = own;
        ownership.dstQueueFamilyIndex = image.queueFamily;
        ownership.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        ownership.dstAccessMask = 0;
        vkCmdPipelineBarrier(
            commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &ownership);
    }
    // The core's next writes to the image, in later submissions, wait for this read.
    vkCmdPipelineBarrier(commands,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
}

bool FrameCompositor::record(VkCommandBuffer commands, const FrameImage& frame, bool upload,
                             const DrawTarget& target, VkExtent2D extent) {
    if (frame.gpu) {
        recordCoreImage(commands, target, extent, frame, *frame.gpu);
        return false;
    }
    const bool complete =
        frame.width > 0 && frame.height > 0 &&
        frame.rgba.size() >= static_cast<std::size_t>(frame.width) * frame.height * 4;
    const bool uploaded = upload && complete;
    if (uploaded) {
        recordUpload(commands, frame);
    } else if (texture_.ready()) {
        bindTexture();
    }
    recordDraw(commands, target, extent, frame, textureImage(), texture_.ready());
    return uploaded;
}

} // namespace opensu::retro
