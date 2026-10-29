#include "presenter.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

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
    instance_ =
        std::make_unique<VulkanInstance>(extensions, options_.validation, options_.negotiation);
    if (!options_.hidden &&
        !SDL_Vulkan_CreateSurface(window_, instance_->handle(), nullptr, &surface_)) {
        throw std::runtime_error{std::string{"Vulkan surface: "} + SDL_GetError()};
    }
    device_ = std::make_unique<VulkanDevice>(*instance_, surface_, options_.negotiation);
    vk_ = device_->handle();
    if (!options_.hidden) {
        format_ = chooseFormat(device_->physical(), surface_).format;
    }
    compositor_ = std::make_unique<FrameCompositor>(
        instance_->handle(), *device_, format_,
        options_.hidden ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        options_.video, options_.librashader);

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
        device_->waitIdle();
        destroyTargets();
        // Without the swapchain extension (hidden) the loader has no entry point to call.
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(vk_, swapchain_, nullptr);
        }
        vkDestroySemaphore(vk_, acquired_, nullptr);
        vkDestroyFence(vk_, done_, nullptr);
        vkDestroyCommandPool(vk_, commandPool_, nullptr);
        compositor_.reset();
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
    offscreenMemory_ = device_->allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkBindImageMemory(vk_, target.image, offscreenMemory_, 0), "vkBindImageMemory");
    targets_.back().view = device_->makeView(target.image, format_);
    targets_.back().framebuffer = makeFramebuffer(targets_.back().view);
}

VkFramebuffer Presenter::makeFramebuffer(VkImageView view) const {
    VkFramebufferCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create.renderPass = compositor_->renderPass();
    create.attachmentCount = 1;
    create.pAttachments = &view;
    create.width = extent_.width;
    create.height = extent_.height;
    create.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    check(vkCreateFramebuffer(vk_, &create, nullptr, &framebuffer), "vkCreateFramebuffer");
    return framebuffer;
}

void Presenter::createSwapchain() {
    stale_ = false;
    VkSurfaceCapabilitiesKHR capabilities;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->physical(), surface_, &capabilities),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    VkExtent2D extent = capabilities.currentExtent;
    // A display turned from its natural orientation (Android landscape) asks for frames drawn
    // turned; they are drawn upright and the system turns them, which it reports as suboptimal.
    VkSurfaceTransformFlagBitsKHR transform = capabilities.currentTransform;
    turnedBySystem_ =
        transform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR &&
        (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0;
    if (turnedBySystem_) {
        transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
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
    lucent::info("retro", "swapchain {}x{}, surface {}x{} turned {}", extent.width, extent.height,
                 capabilities.currentExtent.width, capabilities.currentExtent.height,
                 static_cast<unsigned>(capabilities.currentTransform));
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
        .preTransform = transform,
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
        target.view = device_->makeView(image, format_);
        target.framebuffer = makeFramebuffer(target.view);
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

void Presenter::change(const settings::VideoSettings& video) {
    compositor_->change(video, instance_->handle(), options_.librashader);
}

VkResult Presenter::waitForFrame() {
    return vkWaitForFences(vk_, 1, &done_, VK_TRUE, UINT64_MAX);
}

void Presenter::present(const FrameImage& frame, bool fresh, CoreSync sync) {
    uploadPending_ = uploadPending_ || fresh;
    syncPending_.waits.insert(syncPending_.waits.end(), sync.waits.begin(), sync.waits.end());
    syncPending_.commands.insert(syncPending_.commands.end(), sync.commands.begin(),
                                 sync.commands.end());
    if (sync.signal != VK_NULL_HANDLE) {
        syncPending_.signal = sync.signal;
    }
    if (!options_.hidden && stale_) {
        check(device_->waitIdle(), "vkDeviceWaitIdle");
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
            stale_ = !turnedBySystem_;
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
    if (compositor_->record(commands_, frame, uploadPending_,
                            DrawTarget{targets_[index].image, targets_[index].framebuffer},
                            extent_)) {
        uploadPending_ = false;
    }
    check(vkEndCommandBuffer(commands_), "vkEndCommandBuffer");

    std::vector<VkSemaphore> waits;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> signals;
    if (!options_.hidden) {
        waits.push_back(acquired_);
        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        signals.push_back(targets_[index].drawn);
    }
    for (VkSemaphore wait : syncPending_.waits) {
        waits.push_back(wait);
        waitStages.push_back(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    if (syncPending_.signal != VK_NULL_HANDLE) {
        signals.push_back(syncPending_.signal);
    }
    std::vector<VkCommandBuffer> buffers = std::move(syncPending_.commands);
    buffers.push_back(commands_);
    syncPending_ = {};
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = static_cast<std::uint32_t>(buffers.size());
    submit.pCommandBuffers = buffers.data();
    submit.waitSemaphoreCount = static_cast<std::uint32_t>(waits.size());
    submit.pWaitSemaphores = waits.data();
    submit.pWaitDstStageMask = waitStages.data();
    submit.signalSemaphoreCount = static_cast<std::uint32_t>(signals.size());
    submit.pSignalSemaphores = signals.data();
    check(device_->submit(submit, done_), "vkQueueSubmit");
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
    const VkResult presented = device_->present(presentInfo);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR) {
        stale_ = true;
    } else if (presented == VK_SUBOPTIMAL_KHR) {
        stale_ = !turnedBySystem_;
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
        memory = device_->allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
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
        check(device_->submit(submit, done_), "vkQueueSubmit");
        check(vkWaitForFences(vk_, 1, &done_, VK_TRUE, UINT64_MAX), "vkWaitForFences");
        void* mapped = nullptr;
        check(vkMapMemory(vk_, memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory");
        const auto* bytes = static_cast<const std::uint8_t*>(mapped);
        result.rgba.assign(bytes, bytes + size);
        vkUnmapMemory(vk_, memory);
        // The swapchain is composited opaque; a shader preset may leave alpha below 1.
        for (std::size_t at = 3; at < result.rgba.size(); at += 4) {
            result.rgba[at] = 0xFF;
        }
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
