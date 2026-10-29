// presenter — the runner's window and where its frames go: an SDL window and its events, the
// Vulkan swapchain (or, hidden, an offscreen image that can be read back) and each frame's
// submission. What is drawn into a target is `FrameCompositor`'s, with an overlay over it drawn by
// `OverlayPass`. A surface lost with the window (Android in the background) is let go and made
// again when the window has one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "frame_compositor.hpp"
#include "overlay_pass.hpp"
#include "vulkan_device.hpp"
#include "window_input.hpp"

namespace opensu::retro {

struct PresenterOptions {
    /// An unmapped window and an offscreen image in place of a swapchain.
    bool hidden{false};
    bool fullscreen{false};
    int width{1280};
    int height{800};
    /// The Vulkan validation layer, with its errors counted.
    bool validation{false};
    /// A Vulkan core's context negotiation: the instance and device are made through it.
    const retro_hw_render_context_negotiation_interface_vulkan* negotiation{nullptr};
    /// How the frame is shown.
    settings::VideoSettings video;
    /// The librashader a shader preset runs on.
    std::filesystem::path librashader;
};

/// What a Vulkan core hands over with its frames, each used once: semaphores the drawing waits
/// for, command buffers submitted before the runner's, a semaphore signalled once its image is
/// read.
struct CoreSync {
    std::vector<VkSemaphore> waits;
    std::vector<VkCommandBuffer> commands;
    VkSemaphore signal{VK_NULL_HANDLE};
};

/// An image read back from the GPU, RGBA rows top first.
struct Readback {
    std::vector<std::uint8_t> rgba;
    unsigned width{0};
    unsigned height{0};
};

class Presenter {
  public:
    /// Opens the window and the Vulkan device; throws VulkanError, ShaderError for a shader preset
    /// that cannot run, or std::runtime_error.
    explicit Presenter(const PresenterOptions& options);
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;
    Presenter(Presenter&&) = delete;
    Presenter& operator=(Presenter&&) = delete;
    ~Presenter();

    [[nodiscard]] WindowInput poll();

    /// What an overlay is drawn for: the target's size, its safe area and pixels per dp.
    [[nodiscard]] WindowSurface surface() const;

    /// Draws `frame`, uploading it first when `fresh`, with what a Vulkan core handed over (kept
    /// for the next frame drawn when this one is skipped), and `overlay` over it when given.
    /// Throws VulkanError when drawing fails.
    void present(const FrameImage& frame, bool fresh, CoreSync sync = {},
                 const OverlayImage* overlay = nullptr);

    /// Shows frames under `video` from the next one drawn (`FrameCompositor::change`); a shader
    /// preset that cannot run throws ShaderError and the settings in force stay.
    void change(const settings::VideoSettings& video);

    /// Waits until the last frame submitted is done on the GPU (a core's wait_sync_index).
    [[nodiscard]] VkResult waitForFrame();

    /// Hidden only: the last image presented, opaque as the window shows it; empty before the
    /// first.
    [[nodiscard]] Readback readBack();

    [[nodiscard]] std::size_t validationErrors() const {
        return instance_->validationErrors();
    }
    [[nodiscard]] VulkanDevice& device() {
        return *device_;
    }
    [[nodiscard]] VkInstance instance() const {
        return instance_->handle();
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
    void createOffscreen();
    void createSwapchain();
    [[nodiscard]] VkFramebuffer makeFramebuffer(VkImageView view) const;
    void destroyTargets();
    /// The swapchain, its targets and the window's surface let go, once the surface is lost.
    void dropSurface();

    PresenterOptions options_;
    bool videoUp_{false};
    SDL_Window* window_{nullptr};
    std::unique_ptr<VulkanInstance> instance_;
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    std::unique_ptr<VulkanDevice> device_;
    VkDevice vk_{VK_NULL_HANDLE};

    VkFormat format_{VK_FORMAT_R8G8B8A8_UNORM};
    VkExtent2D extent_{};
    std::unique_ptr<FrameCompositor> compositor_;
    /// Made when the first overlay is drawn.
    std::unique_ptr<OverlayPass> overlay_;
    VkCommandPool commandPool_{VK_NULL_HANDLE};
    VkCommandBuffer commands_{VK_NULL_HANDLE};
    VkFence done_{VK_NULL_HANDLE};
    VkSemaphore acquired_{VK_NULL_HANDLE};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    bool stale_{false};
    /// The swapchain is drawn upright on a turned display, which the system turns: its
    /// suboptimal reports mean only that.
    bool turnedBySystem_{false};
    std::vector<Target> targets_;
    /// The hidden presenter's one target and its memory.
    VkDeviceMemory offscreenMemory_{VK_NULL_HANDLE};
    bool drawnOffscreen_{false};
    /// A fresh frame not yet uploaded, kept across a frame skipped for a stale swapchain.
    bool uploadPending_{false};
    /// A core's hand-over not yet submitted, kept the same way.
    CoreSync syncPending_;
};

} // namespace opensu::retro
