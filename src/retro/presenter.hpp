// presenter — where the runner's frames go: its window (`RunnerWindow`) and events, the
// Vulkan swapchain (or, hidden, offscreen images that can be read back) and each frame's
// submission, up to `framesInFlight` of them on the GPU at once: each frame slot has its command
// buffer, fence and acquire semaphore, and a slot is waited for only when it comes round again, so
// the core runs the next frame while the GPU draws this one. What is drawn into a target is
// `FrameCompositor`'s, with an overlay over it drawn by `OverlayPass`. A surface lost with the
// window (Android in the background) is let go and made again when the window has one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "frame_compositor.hpp"
#include "frame_stages.hpp"
#include "overlay_pass.hpp"
#include "runner_window.hpp"
#include "vulkan_device.hpp"
#include "window_input.hpp"

namespace opensu::retro {

struct PresenterOptions {
    /// An unmapped window and an offscreen image in place of a swapchain.
    bool hidden{false};
    bool fullscreen{false};
    /// Draws into the surface the shell passed (`hosted_runner_window`), not a window of its own.
    bool hosted{false};
    int width{1280};
    int height{800};
    /// Hidden only: pixels per dp, which a window on a screen is told.
    float hiddenDp{1.0F};
    /// The Vulkan validation layer, with its errors counted.
    bool validation{false};
    /// A Vulkan core's context negotiation: the instance and device are made through it.
    const retro_hw_render_context_negotiation_interface_vulkan* negotiation{nullptr};
    /// How the frame is shown.
    settings::VideoSettings video;
    /// The librashader a shader preset runs on.
    std::filesystem::path librashader;
    /// Frames submitted and not yet waited for, at most `framesInFlight`; a GPU core's take one.
    std::size_t inFlight{framesInFlight};
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
    [[nodiscard]] touch::WindowSurface surface() const;
    /// Shrinks the window to `width` x `height`, the size of the picture it shows.
    void fitWindow(unsigned width, unsigned height);
    /// Where `frame` is drawn on the target.
    [[nodiscard]] touch::Box picture(const FrameImage& frame) const;
    /// Puts `finger` on the window's event queue, as the system does for a real one.
    void inject(const touch::Finger& finger);
    /// Android: whether the window still has a surface, which it keeps while it can be seen.
    [[nodiscard]] bool onScreen() const;
    /// What the shell says of the screen a hosted game is on, read by `surface` from now on.
    void place(const touch::WindowSurface& screen) noexcept {
        placed_ = screen;
    }
    /// Whether a frame has reached the target.
    [[nodiscard]] bool shown() const noexcept {
        return shown_;
    }

    /// Draws `frame`, uploading it first when `fresh`, with what a Vulkan core handed over (kept
    /// for the next frame drawn when this one is skipped), and `overlay` over it when given;
    /// the time each step took goes to `stages`. Throws VulkanError when drawing fails.
    void present(const FrameImage& frame, bool fresh, CoreSync sync,
                 const touch::OverlayImage* overlay, FrameStages& stages);

    /// Shows frames under `video` from the next one drawn (`FrameCompositor::change`); a shader
    /// preset that cannot run throws ShaderError and the settings in force stay.
    void change(const settings::VideoSettings& video);
    /// The settings in force.
    [[nodiscard]] const settings::VideoSettings& video() const;
    /// Draws the picture a quarter turned, or upright (`FrameCompositor::turn`).
    void turnPicture(bool turned);
    /// Keeps the picture off the target's top `top` and bottom `bottom` pixels.
    void reserve(std::uint32_t top, std::uint32_t bottom) noexcept;

    /// Waits until every frame submitted is done on the GPU (a core's wait_sync_index).
    [[nodiscard]] VkResult waitForFrame();

    /// The last image presented, opaque as the window shows it, shown again where it is a
    /// swapchain's; empty before the first or while there is nothing to draw into.
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
        /// A hidden target's own memory.
        VkDeviceMemory memory{VK_NULL_HANDLE};
    };
    /// What one frame in flight holds until its fence passes.
    struct Slot {
        VkCommandBuffer commands{VK_NULL_HANDLE};
        VkFence done{VK_NULL_HANDLE};
        /// Signalled when the frame's swapchain image is acquired.
        VkSemaphore acquired{VK_NULL_HANDLE};
    };

    void open();
    /// Everything `open` made, in reverse; safe on a partly opened presenter.
    void release();
    void createOffscreen();
    void createSwapchain();
    [[nodiscard]] VkFramebuffer makeFramebuffer(VkImageView view) const;
    void destroyTargets();
    /// Records copying `image`, drawn and ready to present, into `capture_`.
    void recordCapture(VkCommandBuffer commands, VkImage image);
    /// Waits until every slot's frame is done. Throws VulkanError.
    void waitAll();
    /// The swapchain, its targets and the window's surface let go, once the surface is lost.
    void dropSurface();

    PresenterOptions options_;
    std::unique_ptr<RunnerWindow> window_;
    std::optional<touch::WindowSurface> placed_;
    bool shown_{false};
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
    std::vector<Slot> slots_;
    /// The slot the next frame is recorded into.
    std::size_t slot_{0};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    bool stale_{false};
    /// The swapchain is drawn upright on a turned display, which the system turns: its
    /// suboptimal reports mean only that.
    bool turnedBySystem_{false};
    std::vector<Target> targets_;
    /// The hidden target drawn last; there is one per slot.
    std::optional<std::size_t> drawnOffscreen_;
    /// The swapchain's images can be copied from.
    bool readable_{false};
    /// The frame `readBack` shows again; its pixels are the core's until its next frame.
    std::optional<FrameImage> lastFrame_;
    /// Where the next frame presented is copied while `readBack` waits for it.
    VkBuffer capture_{VK_NULL_HANDLE};
    bool captured_{false};
    /// A fresh frame not yet uploaded, kept across a frame skipped for a stale swapchain.
    bool uploadPending_{false};
    /// A core's hand-over not yet submitted, kept the same way.
    CoreSync syncPending_;
};

} // namespace opensu::retro
