#include "frame_compositor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace opensu::retro {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "frame.vert.inc"
};

constexpr std::uint32_t fragmentCode[] = {
#include "frame.frag.inc"
};

constexpr std::uint32_t sharpBilinearCode[] = {
#include "sharp_bilinear.frag.inc"
};

/// What `sharp_bilinear.frag` takes.
struct SharpScale {
    std::array<float, 2> texels;
    std::array<float, 2> prescale;
};

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

VkShaderModule makeShader(VkDevice device, std::span<const std::uint32_t> code) {
    VkShaderModuleCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create.codeSize = code.size_bytes();
    create.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &create, nullptr, &module), "vkCreateShaderModule");
    return module;
}

void imageBarrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
                  VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                  VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
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

FrameCompositor::FrameCompositor(const VulkanDevice& device, VkFormat format,
                                 VkImageLayout finalLayout, const settings::VideoSettings& video)
    : device_{device}, vk_{device.handle()}, video_{video} {
    try {
        open(format, finalLayout);
    } catch (...) {
        release();
        throw;
    }
}

FrameCompositor::~FrameCompositor() {
    release();
}

void FrameCompositor::open(VkFormat format, VkImageLayout finalLayout) {
    createRenderPass(format, finalLayout);
    createPipeline();
}

void FrameCompositor::release() {
    vkDeviceWaitIdle(vk_);
    destroyTexture();
    vkDestroyDescriptorPool(vk_, descriptorPool_, nullptr);
    vkDestroySampler(vk_, sampler_, nullptr);
    vkDestroyPipeline(vk_, pipeline_, nullptr);
    vkDestroyPipelineLayout(vk_, pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(vk_, setLayout_, nullptr);
    vkDestroyRenderPass(vk_, renderPass_, nullptr);
    descriptorPool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    setLayout_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;
}

void FrameCompositor::createRenderPass(VkFormat format, VkImageLayout finalLayout) {
    const VkAttachmentDescription colour{
        .flags = 0,
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = finalLayout,
    };
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    std::array<VkSubpassDependency, 2> dependencies{};
    // The swapchain image is free once acquired; the offscreen one once read back.
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create.attachmentCount = 1;
    create.pAttachments = &colour;
    create.subpassCount = 1;
    create.pSubpasses = &subpass;
    create.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    create.pDependencies = dependencies.data();
    check(vkCreateRenderPass(vk_, &create, nullptr, &renderPass_), "vkCreateRenderPass");
}

void FrameCompositor::createPipeline() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo setLayout{};
    setLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayout.bindingCount = 1;
    setLayout.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(vk_, &setLayout, nullptr, &setLayout_),
          "vkCreateDescriptorSetLayout");
    const VkPushConstantRange scale{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SharpScale)};
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &setLayout_;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &scale;
    check(vkCreatePipelineLayout(vk_, &layout, nullptr, &pipelineLayout_),
          "vkCreatePipelineLayout");

    const VkShaderModule vertex = makeShader(vk_, vertexCode);
    VkShaderModule fragment = VK_NULL_HANDLE;
    try {
        fragment = video_.sampling == settings::Sampling::SharpBilinear
                       ? makeShader(vk_, sharpBilinearCode)
                       : makeShader(vk_, fragmentCode);
    } catch (...) {
        vkDestroyShaderModule(vk_, vertex, nullptr);
        throw;
    }
    const auto stage = [](VkShaderStageFlagBits kind, VkShaderModule module) {
        return VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = kind,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };
    };
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages{
        stage(VK_SHADER_STAGE_VERTEX_BIT, vertex), stage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment)};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0F;
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0F,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamic.pDynamicStates = dynamicStates.data();
    VkGraphicsPipelineCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create.stageCount = static_cast<std::uint32_t>(stages.size());
    create.pStages = stages.data();
    create.pVertexInputState = &vertexInput;
    create.pInputAssemblyState = &assembly;
    create.pViewportState = &viewport;
    create.pRasterizationState = &raster;
    create.pMultisampleState = &multisample;
    create.pColorBlendState = &blend;
    create.pDynamicState = &dynamic;
    create.layout = pipelineLayout_;
    create.renderPass = renderPass_;
    const VkResult made =
        vkCreateGraphicsPipelines(vk_, VK_NULL_HANDLE, 1, &create, nullptr, &pipeline_);
    vkDestroyShaderModule(vk_, vertex, nullptr);
    vkDestroyShaderModule(vk_, fragment, nullptr);
    check(made, "vkCreateGraphicsPipelines");

    const VkFilter filter =
        video_.sampling == settings::Sampling::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = filter;
    sampler.minFilter = filter;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(vk_, &sampler, nullptr, &sampler_), "vkCreateSampler");
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = 1;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &size;
    check(vkCreateDescriptorPool(vk_, &pool, nullptr, &descriptorPool_), "vkCreateDescriptorPool");
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = descriptorPool_;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &setLayout_;
    check(vkAllocateDescriptorSets(vk_, &allocate, &descriptorSet_), "vkAllocateDescriptorSets");
}

void FrameCompositor::ensureTexture(unsigned width, unsigned height) {
    if (texture_ != VK_NULL_HANDLE && width == textureWidth_ && height == textureHeight_) {
        return;
    }
    check(vkDeviceWaitIdle(vk_), "vkDeviceWaitIdle");
    destroyTexture();
    const VkImageCreateInfo create =
        imageInfo(VK_FORMAT_R8G8B8A8_UNORM, {width, height},
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    check(vkCreateImage(vk_, &create, nullptr, &texture_), "vkCreateImage");
    VkMemoryRequirements needs{};
    vkGetImageMemoryRequirements(vk_, texture_, &needs);
    textureMemory_ = device_.allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkBindImageMemory(vk_, texture_, textureMemory_, 0), "vkBindImageMemory");
    textureView_ = device_.makeView(texture_, VK_FORMAT_R8G8B8A8_UNORM);

    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = static_cast<VkDeviceSize>(width) * height * 4;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(vk_, &buffer, nullptr, &staging_), "vkCreateBuffer");
    vkGetBufferMemoryRequirements(vk_, staging_, &needs);
    stagingMemory_ = device_.allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkBindBufferMemory(vk_, staging_, stagingMemory_, 0), "vkBindBufferMemory");
    check(vkMapMemory(vk_, stagingMemory_, 0, VK_WHOLE_SIZE, 0, &stagingMapped_), "vkMapMemory");
    textureWidth_ = width;
    textureHeight_ = height;
    textureReady_ = false;

    VkDescriptorImageInfo image{sampler_, textureView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    vkUpdateDescriptorSets(vk_, 1, &write, 0, nullptr);
}

void FrameCompositor::destroyTexture() {
    if (stagingMapped_ != nullptr) {
        vkUnmapMemory(vk_, stagingMemory_);
        stagingMapped_ = nullptr;
    }
    vkDestroyBuffer(vk_, staging_, nullptr);
    vkFreeMemory(vk_, stagingMemory_, nullptr);
    vkDestroyImageView(vk_, textureView_, nullptr);
    vkDestroyImage(vk_, texture_, nullptr);
    vkFreeMemory(vk_, textureMemory_, nullptr);
    staging_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    textureView_ = VK_NULL_HANDLE;
    texture_ = VK_NULL_HANDLE;
    textureMemory_ = VK_NULL_HANDLE;
    textureReady_ = false;
}

void FrameCompositor::recordUpload(VkCommandBuffer commands, const FrameImage& frame) {
    ensureTexture(frame.width, frame.height);
    std::memcpy(stagingMapped_, frame.rgba.data(),
                static_cast<std::size_t>(frame.width) * frame.height * 4);
    imageBarrier(
        commands, texture_,
        textureReady_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        textureReady_ ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        textureReady_ ? VK_ACCESS_SHADER_READ_BIT : 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {frame.width, frame.height, 1};
    vkCmdCopyBufferToImage(commands, staging_, texture_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    imageBarrier(commands, texture_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_SHADER_READ_BIT);
    textureReady_ = true;
}

void FrameCompositor::recordDraw(VkCommandBuffer commands, VkFramebuffer target, VkExtent2D extent,
                                 const FrameImage& frame) {
    VkClearValue black{};
    black.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = renderPass_;
    begin.framebuffer = target;
    begin.renderArea.extent = extent;
    begin.clearValueCount = 1;
    begin.pClearValues = &black;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    const VkRect2D rect = placeFrame(extent, frame, video_);
    const VkRect2D scissor = clipped(rect, extent);
    if (textureReady_ && scissor.extent.width > 0 && scissor.extent.height > 0) {
        VkViewport viewport{};
        viewport.x = static_cast<float>(rect.offset.x);
        viewport.y = static_cast<float>(rect.offset.y);
        viewport.width = static_cast<float>(rect.extent.width);
        viewport.height = static_cast<float>(rect.extent.height);
        viewport.maxDepth = 1.0F;
        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                                &descriptorSet_, 0, nullptr);
        vkCmdSetViewport(commands, 0, 1, &viewport);
        vkCmdSetScissor(commands, 0, 1, &scissor);
        if (video_.sampling == settings::Sampling::SharpBilinear) {
            const auto texelsWide = static_cast<float>(textureWidth_);
            const auto texelsHigh = static_cast<float>(textureHeight_);
            const SharpScale scale{{texelsWide, texelsHigh},
                                   {wholeScale(viewport.width, texelsWide, false),
                                    wholeScale(viewport.height, texelsHigh, false)}};
            vkCmdPushConstants(commands, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(scale), &scale);
        }
        vkCmdDraw(commands, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commands);
}

bool FrameCompositor::record(VkCommandBuffer commands, const FrameImage& frame, bool upload,
                             VkFramebuffer target, VkExtent2D extent) {
    const bool complete =
        frame.width > 0 && frame.height > 0 &&
        frame.rgba.size() >= static_cast<std::size_t>(frame.width) * frame.height * 4;
    const bool uploaded = upload && complete;
    if (uploaded) {
        recordUpload(commands, frame);
    }
    recordDraw(commands, target, extent, frame);
    return uploaded;
}

} // namespace opensu::retro
