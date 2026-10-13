// presenter — the runner's window and what it shows: an SDL window and its events, and the core's
// frame drawn with Vulkan, scaled to the window with its aspect kept and letterboxed in black. A
// hidden presenter draws into an offscreen image that can be read back instead of a swapchain.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "vulkan_device.hpp"

namespace opensu::retro {

struct PresenterOptions {
    /// An unmapped window and an offscreen image in place of a swapchain.
    bool hidden{false};
    bool fullscreen{false};
    int width{1280};
    int height{800};
    /// The Vulkan validation layer, with its errors counted.
    bool validation{false};
};

/// What the window reported since the last poll.
struct WindowInput {
    struct Key {
        /// An SDL_Scancode.
        int scancode{0};
        bool down{false};
    };

    bool quit{false};
    std::vector<Key> keys;
};

/// A frame to show: rows of `width` RGBA pixels, top first, displayed `aspect` wide per unit high.
struct FrameImage {
    std::span<const std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
    float aspect{0.0F};
};

/// An image read back from the GPU, RGBA rows top first.
struct Readback {
    std::vector<std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
};

class Presenter {
  public:
    /// Opens the window and the Vulkan device; throws VulkanError or std::runtime_error.
    explicit Presenter(const PresenterOptions& options);
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;
    Presenter(Presenter&&) = delete;
    Presenter& operator=(Presenter&&) = delete;
    ~Presenter();

    [[nodiscard]] WindowInput poll();

    /// Draws `frame`, uploading it first when `fresh`. Throws VulkanError when drawing fails.
    void present(const FrameImage& frame, bool fresh);

    /// Hidden only: the last image presented; empty before the first.
    [[nodiscard]] Readback readBack();

    [[nodiscard]] std::size_t validationErrors() const {
        return instance_->validationErrors();
    }
    [[nodiscard]] const VulkanDevice& device() const {
        return *device_;
    }

  private:
    struct Target {
        VkImage image{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
        /// Signalled when drawing into this swapchain image is done.
        VkSemaphore drawn{VK_NULL_HANDLE};
    };

    void open();
    /// Everything `open` made, in reverse; safe on a partly opened presenter.
    void release();
    void createRenderPass();
    void createPipeline();
    void createOffscreen();
    void createSwapchain();
    void destroyTargets();
    void ensureTexture(unsigned width, unsigned height);
    void destroyTexture();
    void recordUpload(VkCommandBuffer commands, const FrameImage& frame);
    void recordDraw(VkCommandBuffer commands, const Target& target, float aspect);
    [[nodiscard]] VkImageView makeView(VkImage image, VkFormat format) const;
    [[nodiscard]] VkDeviceMemory allocate(VkMemoryRequirements needs,
                                          VkMemoryPropertyFlags properties) const;

    PresenterOptions options_;
    bool videoUp_{false};
    SDL_Window* window_{nullptr};
    std::unique_ptr<VulkanInstance> instance_;
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    std::unique_ptr<VulkanDevice> device_;
    VkDevice vk_{VK_NULL_HANDLE};

    VkFormat format_{VK_FORMAT_R8G8B8A8_UNORM};
    VkExtent2D extent_{};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkDescriptorSetLayout setLayout_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
    VkCommandPool commandPool_{VK_NULL_HANDLE};
    VkCommandBuffer commands_{VK_NULL_HANDLE};
    VkFence done_{VK_NULL_HANDLE};
    VkSemaphore acquired_{VK_NULL_HANDLE};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    bool stale_{false};
    std::vector<Target> targets_;
    /// The hidden presenter's one target and its memory.
    VkDeviceMemory offscreenMemory_{VK_NULL_HANDLE};
    bool drawnOffscreen_{false};

    VkImage texture_{VK_NULL_HANDLE};
    VkDeviceMemory textureMemory_{VK_NULL_HANDLE};
    VkImageView textureView_{VK_NULL_HANDLE};
    unsigned textureWidth_{0};
    unsigned textureHeight_{0};
    bool textureReady_{false};
    /// A fresh frame not yet uploaded, kept across a frame skipped for a stale swapchain.
    bool uploadPending_{false};
    VkBuffer staging_{VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory_{VK_NULL_HANDLE};
    void* stagingMapped_{nullptr};
};

} // namespace opensu::retro
