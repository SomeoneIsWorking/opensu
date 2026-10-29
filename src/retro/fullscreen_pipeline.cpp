#include "fullscreen_pipeline.hpp"

#include <array>

#include "vulkan_device.hpp"

namespace opensu::retro {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "frame.vert.inc"
};

} // namespace

SampledInput::SampledInput(VkDevice device, VkFilter filter) : device_{device} {
    try {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo setLayout{};
        setLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayout.bindingCount = 1;
        setLayout.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(device_, &setLayout, nullptr, &setLayout_),
              "vkCreateDescriptorSetLayout");
        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = filter;
        sampler.minFilter = filter;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(device_, &sampler, nullptr, &sampler_), "vkCreateSampler");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &size;
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_), "vkCreateDescriptorPool");
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool_;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &setLayout_;
        check(vkAllocateDescriptorSets(device_, &allocate, &set_), "vkAllocateDescriptorSets");
    } catch (...) {
        release();
        throw;
    }
}

SampledInput::~SampledInput() {
    release();
}

void SampledInput::release() {
    vkDestroyDescriptorPool(device_, pool_, nullptr);
    vkDestroySampler(device_, sampler_, nullptr);
    vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
    pool_ = VK_NULL_HANDLE;
    set_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    setLayout_ = VK_NULL_HANDLE;
}

void SampledInput::bind(VkImageView view, VkImageLayout layout) {
    bound_ = view;
    VkDescriptorImageInfo image{sampler_, view, layout};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

VkRenderPass makeColourPass(VkDevice device, VkFormat format, VkAttachmentLoadOp load,
                            VkImageLayout initial, VkImageLayout final) {
    const bool loads = load == VK_ATTACHMENT_LOAD_OP_LOAD;
    const VkAttachmentDescription colour{
        .flags = 0,
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = load,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = loads ? initial : VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = final,
    };
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    std::array<VkSubpassDependency, 2> dependencies{};
    // The swapchain image is free once acquired; the offscreen one once read back; a loaded one
    // once the drawing before is done.
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = loads ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | (loads ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : 0);
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
    VkRenderPass pass = VK_NULL_HANDLE;
    check(vkCreateRenderPass(device, &create, nullptr, &pass), "vkCreateRenderPass");
    return pass;
}

VkPipeline makeFullscreenPipeline(VkDevice device, VkRenderPass pass, VkPipelineLayout layout,
                                  std::span<const std::uint32_t> fragment, bool blended) {
    const VkShaderModule vertexModule = makeShader(device, vertexCode);
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    try {
        fragmentModule = makeShader(device, fragment);
    } catch (...) {
        vkDestroyShaderModule(device, vertexModule, nullptr);
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
        stage(VK_SHADER_STAGE_VERTEX_BIT, vertexModule),
        stage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule)};
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
    if (blended) {
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
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
    create.layout = layout;
    create.renderPass = pass;
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult made =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create, nullptr, &pipeline);
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
    check(made, "vkCreateGraphicsPipelines");
    return pipeline;
}

} // namespace opensu::retro
