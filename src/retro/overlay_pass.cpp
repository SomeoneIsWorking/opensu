#include "overlay_pass.hpp"

#include "fullscreen_pipeline.hpp"

namespace opensu::retro {
namespace {

constexpr std::uint32_t overlayCode[] = {
#include "overlay.frag.inc"
};

} // namespace

OverlayPass::OverlayPass(VulkanDevice& device, VkFormat format, VkImageLayout layout)
    : device_{device}, vk_{device.handle()}, texture_{device, 0},
      input_{device.handle(), VK_FILTER_NEAREST} {
    try {
        pass_ = makeColourPass(vk_, format, VK_ATTACHMENT_LOAD_OP_LOAD, layout, layout);
        const VkDescriptorSetLayout setLayout = input_.setLayout();
        VkPipelineLayoutCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        create.setLayoutCount = 1;
        create.pSetLayouts = &setLayout;
        check(vkCreatePipelineLayout(vk_, &create, nullptr, &pipelineLayout_),
              "vkCreatePipelineLayout");
        pipeline_ = makeFullscreenPipeline(vk_, pass_, pipelineLayout_, overlayCode, true);
    } catch (...) {
        release();
        throw;
    }
}

OverlayPass::~OverlayPass() {
    release();
}

void OverlayPass::release() {
    device_.waitIdle();
    vkDestroyPipeline(vk_, pipeline_, nullptr);
    vkDestroyPipelineLayout(vk_, pipelineLayout_, nullptr);
    vkDestroyRenderPass(vk_, pass_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    pass_ = VK_NULL_HANDLE;
}

void OverlayPass::recordUpload(VkCommandBuffer commands, const OverlayImage& overlay) {
    const bool remade = texture_.ensure(overlay.width, overlay.height);
    if (remade) {
        input_.bind(texture_.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if (remade || overlay.changed) {
        texture_.recordUpload(commands, overlay.rgba);
    }
}

void OverlayPass::recordDraw(VkCommandBuffer commands, VkFramebuffer framebuffer,
                             VkExtent2D extent) const {
    if (!texture_.ready()) {
        return;
    }
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = pass_;
    begin.framebuffer = framebuffer;
    begin.renderArea.extent = extent;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.maxDepth = 1.0F;
    const VkRect2D scissor{{0, 0}, extent};
    const VkDescriptorSet set = input_.set();
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &set,
                            0, nullptr);
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    vkCmdDraw(commands, 3, 1, 0, 0);
    vkCmdEndRenderPass(commands);
}

} // namespace opensu::retro
