#include "reduced_output.hpp"

namespace opensu::retro {
namespace {

constexpr std::uint32_t sampledCode[] = {
#include "sampled.frag.inc"
};

} // namespace

ReducedOutput::ReducedOutput(VulkanDevice& device, VkFormat format, VkRenderPass pass)
    : device_{device}, vk_{device.handle()}, format_{format} {
    try {
        input_ = std::make_unique<SampledInput>(vk_, VK_FILTER_LINEAR);
        const VkDescriptorSetLayout setLayout = input_->setLayout();
        VkPipelineLayoutCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        create.setLayoutCount = 1;
        create.pSetLayouts = &setLayout;
        check(vkCreatePipelineLayout(vk_, &create, nullptr, &pipelineLayout_),
              "vkCreatePipelineLayout");
        pipeline_ = makeFullscreenPipeline(vk_, pass, pipelineLayout_, sampledCode, false);
    } catch (...) {
        release();
        throw;
    }
}

ReducedOutput::~ReducedOutput() {
    release();
}

void ReducedOutput::release() {
    device_.waitIdle();
    destroyImage();
    vkDestroyPipeline(vk_, pipeline_, nullptr);
    vkDestroyPipelineLayout(vk_, pipelineLayout_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    input_.reset();
}

void ReducedOutput::destroyImage() {
    vkDestroyImageView(vk_, view_, nullptr);
    vkDestroyImage(vk_, image_, nullptr);
    vkFreeMemory(vk_, memory_, nullptr);
    view_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    extent_ = {};
}

ChainImage ReducedOutput::recordBegin(VkCommandBuffer commands, VkExtent2D extent) {
    if (image_ == VK_NULL_HANDLE || extent.width != extent_.width ||
        extent.height != extent_.height) {
        check(device_.waitIdle(), "vkDeviceWaitIdle");
        destroyImage();
        const VkImageCreateInfo create = imageInfo(
            format_, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        check(vkCreateImage(vk_, &create, nullptr, &image_), "vkCreateImage");
        VkMemoryRequirements needs{};
        vkGetImageMemoryRequirements(vk_, image_, &needs);
        memory_ = device_.allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(vk_, image_, memory_, 0), "vkBindImageMemory");
        view_ = device_.makeView(image_, format_);
        input_->bind(view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        extent_ = extent;
    }
    // The chain draws all of it, so what it held is not kept.
    imageBarrier(commands, image_, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    return ChainImage{image_, format_, extent_};
}

void ReducedOutput::recordEnd(VkCommandBuffer commands) const {
    imageBarrier(commands, image_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_SHADER_READ_BIT);
}

void ReducedOutput::recordDraw(VkCommandBuffer commands) const {
    const VkDescriptorSet set = input_->set();
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &set,
                            0, nullptr);
    vkCmdDraw(commands, 3, 1, 0, 0);
}

} // namespace opensu::retro
