#include "presenter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "frame.vert.inc"
};

constexpr std::uint32_t fragmentCode[] = {
#include "frame.frag.inc"
};

/// The swapchain format with the frame's bytes passed through unconverted, else the first offered.
VkSurfaceFormatKHR chooseFormat(VkPhysicalDevice device, VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());
    if (formats.empty()) {
        throw VulkanError{"the window offers no surface formats"};
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_UNORM ||
             format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

/// Frames are paced by the runner's clock, so presenting should not wait for vblank.
VkPresentModeKHR choosePresentMode(VkPhysicalDevice device, VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.data());
    for (const VkPresentModeKHR wanted :
         {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
        if (std::ranges::find(modes, wanted) != modes.end()) {
            return wanted;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

/// The largest rectangle of `aspect` centred in `extent`.
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

/// A single-sampled, single-level 2D image description.
VkImageCreateInfo imageInfo(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    return VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
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

Presenter::Presenter(const PresenterOptions& options) : options_{options} {
    try {
        open();
    } catch (...) {
        release();
        throw;
    }
}

Presenter::~Presenter() {
    release();
}

void Presenter::open() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        throw std::runtime_error{std::string{"video: "} + SDL_GetError()};
    }
    videoUp_ = true;
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
    if (!options_.hidden) {
        flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        if (options_.fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        }
    }
    window_ = SDL_CreateWindow("openSU", options_.width, options_.height, flags);
    if (window_ == nullptr) {
        throw std::runtime_error{std::string{"window: "} + SDL_GetError()};
    }
    std::span<const char* const> extensions;
    if (!options_.hidden) {
        Uint32 count = 0;
        const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
        if (names == nullptr) {
            throw std::runtime_error{std::string{"Vulkan window: "} + SDL_GetError()};
        }
        extensions = {names, count};
    }
    instance_ = std::make_unique<VulkanInstance>(extensions, options_.validation);
    if (!options_.hidden &&
        !SDL_Vulkan_CreateSurface(window_, instance_->handle(), nullptr, &surface_)) {
        throw std::runtime_error{std::string{"Vulkan surface: "} + SDL_GetError()};
    }
    device_ = std::make_unique<VulkanDevice>(*instance_, surface_);
    vk_ = device_->handle();
    if (!options_.hidden) {
        format_ = chooseFormat(device_->physical(), surface_).format;
    }
    createRenderPass();
    createPipeline();

    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = device_->queueFamily();
    check(vkCreateCommandPool(vk_, &pool, nullptr, &commandPool_), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = commandPool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    check(vkAllocateCommandBuffers(vk_, &allocate, &commands_), "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    check(vkCreateFence(vk_, &fence, nullptr, &done_), "vkCreateFence");
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    check(vkCreateSemaphore(vk_, &semaphore, nullptr, &acquired_), "vkCreateSemaphore");

    if (options_.hidden) {
        createOffscreen();
    } else {
        createSwapchain();
    }
}

void Presenter::release() {
    if (vk_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vk_);
        destroyTexture();
        destroyTargets();
        // Without the swapchain extension (hidden) the loader has no entry point to call.
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(vk_, swapchain_, nullptr);
        }
        vkDestroySemaphore(vk_, acquired_, nullptr);
        vkDestroyFence(vk_, done_, nullptr);
        vkDestroyCommandPool(vk_, commandPool_, nullptr);
        vkDestroyDescriptorPool(vk_, descriptorPool_, nullptr);
        vkDestroySampler(vk_, sampler_, nullptr);
        vkDestroyPipeline(vk_, pipeline_, nullptr);
        vkDestroyPipelineLayout(vk_, pipelineLayout_, nullptr);
        vkDestroyDescriptorSetLayout(vk_, setLayout_, nullptr);
        vkDestroyRenderPass(vk_, renderPass_, nullptr);
        vk_ = VK_NULL_HANDLE;
    }
    device_.reset();
    if (surface_ != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(instance_->handle(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    instance_.reset();
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (videoUp_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        videoUp_ = false;
    }
}

WindowInput Presenter::poll() {
    WindowInput input;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            input.quit = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            stale_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (!event.key.repeat) {
                input.keys.push_back(
                    {static_cast<int>(event.key.scancode), event.type == SDL_EVENT_KEY_DOWN});
            }
            break;
        default:
            break;
        }
    }
    return input;
}

void Presenter::createRenderPass() {
    const VkAttachmentDescription colour{
        .flags = 0,
        .format = format_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = options_.hidden ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                       : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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

void Presenter::createPipeline() {
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
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &setLayout_;
    check(vkCreatePipelineLayout(vk_, &layout, nullptr, &pipelineLayout_),
          "vkCreatePipelineLayout");

    const VkShaderModule vertex = makeShader(vk_, vertexCode);
    VkShaderModule fragment = VK_NULL_HANDLE;
    try {
        fragment = makeShader(vk_, fragmentCode);
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

    // Nearest, so the core's pixels stay sharp.
    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;
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

VkImageView Presenter::makeView(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create.image = image;
    create.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create.format = format;
    create.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    check(vkCreateImageView(vk_, &create, nullptr, &view), "vkCreateImageView");
    return view;
}

VkDeviceMemory Presenter::allocate(VkMemoryRequirements needs,
                                   VkMemoryPropertyFlags properties) const {
    VkMemoryAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = needs.size;
    allocate.memoryTypeIndex = device_->memoryType(needs.memoryTypeBits, properties);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    check(vkAllocateMemory(vk_, &allocate, nullptr, &memory), "vkAllocateMemory");
    return memory;
}

void Presenter::createOffscreen() {
    extent_ = {static_cast<std::uint32_t>(options_.width),
               static_cast<std::uint32_t>(options_.height)};
    Target target;
    const VkImageCreateInfo create = imageInfo(
        format_, extent_, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    check(vkCreateImage(vk_, &create, nullptr, &target.image), "vkCreateImage");
    targets_.push_back(target);
    VkMemoryRequirements needs{};
    vkGetImageMemoryRequirements(vk_, target.image, &needs);
    offscreenMemory_ = allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkBindImageMemory(vk_, target.image, offscreenMemory_, 0), "vkBindImageMemory");
    targets_.back().view = makeView(target.image, format_);
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = renderPass_;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &targets_.back().view;
    framebuffer.width = extent_.width;
    framebuffer.height = extent_.height;
    framebuffer.layers = 1;
    check(vkCreateFramebuffer(vk_, &framebuffer, nullptr, &targets_.back().framebuffer),
          "vkCreateFramebuffer");
}

void Presenter::createSwapchain() {
    stale_ = false;
    VkSurfaceCapabilitiesKHR capabilities;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->physical(), surface_, &capabilities),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        extent.width =
            std::clamp(static_cast<std::uint32_t>(std::max(width, 0)),
                       capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height =
            std::clamp(static_cast<std::uint32_t>(std::max(height, 0)),
                       capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    destroyTargets();
    if (extent.width == 0 || extent.height == 0) {
        // Minimised: nothing to draw into until the window comes back.
        stale_ = true;
        return;
    }
    extent_ = extent;
    std::uint32_t images = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        images = std::min(images, capabilities.maxImageCount);
    }
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha & alpha) == 0) {
        alpha = static_cast<VkCompositeAlphaFlagBitsKHR>(
            capabilities.supportedCompositeAlpha & (~capabilities.supportedCompositeAlpha + 1));
    }
    const VkSurfaceFormatKHR format = chooseFormat(device_->physical(), surface_);
    const VkSwapchainCreateInfoKHR create{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = images,
        .imageFormat = format_,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = alpha,
        .presentMode = choosePresentMode(device_->physical(), surface_),
        .clipped = VK_TRUE,
        .oldSwapchain = swapchain_,
    };
    VkSwapchainKHR replaced = VK_NULL_HANDLE;
    const VkResult made = vkCreateSwapchainKHR(vk_, &create, nullptr, &replaced);
    vkDestroySwapchainKHR(vk_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    check(made, "vkCreateSwapchainKHR");
    swapchain_ = replaced;

    std::uint32_t count = 0;
    vkGetSwapchainImagesKHR(vk_, swapchain_, &count, nullptr);
    std::vector<VkImage> swapchainImages(count);
    vkGetSwapchainImagesKHR(vk_, swapchain_, &count, swapchainImages.data());
    for (VkImage image : swapchainImages) {
        targets_.push_back(Target{image, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE});
        Target& target = targets_.back();
        target.view = makeView(image, format_);
        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = renderPass_;
        framebuffer.attachmentCount = 1;
        framebuffer.pAttachments = &target.view;
        framebuffer.width = extent_.width;
        framebuffer.height = extent_.height;
        framebuffer.layers = 1;
        check(vkCreateFramebuffer(vk_, &framebuffer, nullptr, &target.framebuffer),
              "vkCreateFramebuffer");
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        check(vkCreateSemaphore(vk_, &semaphore, nullptr, &target.drawn), "vkCreateSemaphore");
    }
}

void Presenter::destroyTargets() {
    for (const Target& target : targets_) {
        vkDestroyFramebuffer(vk_, target.framebuffer, nullptr);
        vkDestroyImageView(vk_, target.view, nullptr);
        vkDestroySemaphore(vk_, target.drawn, nullptr);
        if (options_.hidden) {
            vkDestroyImage(vk_, target.image, nullptr);
        }
    }
    targets_.clear();
    vkFreeMemory(vk_, offscreenMemory_, nullptr);
    offscreenMemory_ = VK_NULL_HANDLE;
}

void Presenter::ensureTexture(unsigned width, unsigned height) {
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
    textureMemory_ = allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkBindImageMemory(vk_, texture_, textureMemory_, 0), "vkBindImageMemory");
    textureView_ = makeView(texture_, VK_FORMAT_R8G8B8A8_UNORM);

    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = static_cast<VkDeviceSize>(width) * height * 4;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(vk_, &buffer, nullptr, &staging_), "vkCreateBuffer");
    vkGetBufferMemoryRequirements(vk_, staging_, &needs);
    stagingMemory_ =
        allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

void Presenter::destroyTexture() {
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

void Presenter::recordUpload(VkCommandBuffer commands, const FrameImage& frame) {
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

void Presenter::recordDraw(VkCommandBuffer commands, const Target& target, float aspect) {
    VkClearValue black{};
    black.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = renderPass_;
    begin.framebuffer = target.framebuffer;
    begin.renderArea.extent = extent_;
    begin.clearValueCount = 1;
    begin.pClearValues = &black;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    if (textureReady_) {
        const VkRect2D rect = letterbox(extent_, aspect);
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
        vkCmdSetScissor(commands, 0, 1, &rect);
        vkCmdDraw(commands, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commands);
}

void Presenter::present(const FrameImage& frame, bool fresh) {
    uploadPending_ = uploadPending_ || fresh;
    if (!options_.hidden && stale_) {
        check(vkDeviceWaitIdle(vk_), "vkDeviceWaitIdle");
        createSwapchain();
    }
    if (targets_.empty()) {
        return;
    }
    check(vkWaitForFences(vk_, 1, &done_, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    std::uint32_t index = 0;
    if (!options_.hidden) {
        const VkResult acquired =
            vkAcquireNextImageKHR(vk_, swapchain_, UINT64_MAX, acquired_, VK_NULL_HANDLE, &index);
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
            stale_ = true;
            return;
        }
        if (acquired == VK_SUBOPTIMAL_KHR) {
            stale_ = true;
        } else {
            check(acquired, "vkAcquireNextImageKHR");
        }
    }
    check(vkResetFences(vk_, 1, &done_), "vkResetFences");
    check(vkResetCommandBuffer(commands_, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(commands_, &begin), "vkBeginCommandBuffer");
    const bool complete =
        frame.width > 0 && frame.height > 0 &&
        frame.rgba.size() >= static_cast<std::size_t>(frame.width) * frame.height * 4;
    if (uploadPending_ && complete) {
        recordUpload(commands_, frame);
        uploadPending_ = false;
    }
    const float aspect = frame.aspect > 0.0F || frame.height == 0
                             ? frame.aspect
                             : static_cast<float>(frame.width) / static_cast<float>(frame.height);
    recordDraw(commands_, targets_[index], aspect);
    check(vkEndCommandBuffer(commands_), "vkEndCommandBuffer");

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands_;
    if (!options_.hidden) {
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquired_;
        submit.pWaitDstStageMask = &waitStage;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &targets_[index].drawn;
    }
    check(vkQueueSubmit(device_->queue(), 1, &submit, done_), "vkQueueSubmit");
    if (options_.hidden) {
        drawnOffscreen_ = true;
        return;
    }
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &targets_[index].drawn;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &index;
    const VkResult presented = vkQueuePresentKHR(device_->queue(), &presentInfo);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        stale_ = true;
    } else {
        check(presented, "vkQueuePresentKHR");
    }
}

Readback Presenter::readBack() {
    if (!options_.hidden || !drawnOffscreen_) {
        return {};
    }
    check(vkWaitForFences(vk_, 1, &done_, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    const VkDeviceSize size = static_cast<VkDeviceSize>(extent_.width) * extent_.height * 4;
    VkBufferCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = size;
    create.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    check(vkCreateBuffer(vk_, &create, nullptr, &buffer), "vkCreateBuffer");
    VkDeviceMemory memory = VK_NULL_HANDLE;
    Readback result;
    try {
        VkMemoryRequirements needs{};
        vkGetBufferMemoryRequirements(vk_, buffer, &needs);
        memory = allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkBindBufferMemory(vk_, buffer, memory, 0), "vkBindBufferMemory");
        check(vkResetFences(vk_, 1, &done_), "vkResetFences");
        check(vkResetCommandBuffer(commands_, 0), "vkResetCommandBuffer");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(commands_, &begin), "vkBeginCommandBuffer");
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {extent_.width, extent_.height, 1};
        vkCmdCopyImageToBuffer(commands_, targets_.front().image,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
        check(vkEndCommandBuffer(commands_), "vkEndCommandBuffer");
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands_;
        check(vkQueueSubmit(device_->queue(), 1, &submit, done_), "vkQueueSubmit");
        check(vkWaitForFences(vk_, 1, &done_, VK_TRUE, UINT64_MAX), "vkWaitForFences");
        void* mapped = nullptr;
        check(vkMapMemory(vk_, memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory");
        const auto* bytes = static_cast<const std::uint8_t*>(mapped);
        result.rgba.assign(bytes, bytes + size);
        vkUnmapMemory(vk_, memory);
        result.width = extent_.width;
        result.height = extent_.height;
    } catch (...) {
        vkDestroyBuffer(vk_, buffer, nullptr);
        vkFreeMemory(vk_, memory, nullptr);
        throw;
    }
    vkDestroyBuffer(vk_, buffer, nullptr);
    vkFreeMemory(vk_, memory, nullptr);
    return result;
}

} // namespace opensu::retro
