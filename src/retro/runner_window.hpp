// runner_window — the window the runner's frames go to and its events, whoever owns it: its own
// SDL window (`SdlRunnerWindow`), or on Android a surface the shell passed it, in the shell's
// window (`hosted_runner_window`). The presenter draws through this and knows neither.
#pragma once

#include <memory>
#include <span>

#include <vulkan/vulkan.h>

#include "touch/touch_surface.hpp"
#include "window_input.hpp"

namespace opensu::retro {

class RunnerWindow {
  public:
    RunnerWindow() = default;
    RunnerWindow(const RunnerWindow&) = delete;
    RunnerWindow& operator=(const RunnerWindow&) = delete;
    RunnerWindow(RunnerWindow&&) = delete;
    RunnerWindow& operator=(RunnerWindow&&) = delete;
    virtual ~RunnerWindow() = default;

    /// The instance extensions its surfaces need.
    [[nodiscard]] virtual std::span<const char* const> surfaceExtensions() const = 0;
    /// A surface for the first swapchain, waiting for the window to have one; throws
    /// std::runtime_error when it cannot be made.
    [[nodiscard]] virtual VkSurfaceKHR openSurface(VkInstance instance) = 0;
    /// A surface made again after one was lost, or VK_NULL_HANDLE while the window has none.
    [[nodiscard]] virtual VkSurfaceKHR makeSurface(VkInstance instance) = 0;
    virtual void destroySurface(VkInstance instance, VkSurfaceKHR surface) = 0;

    [[nodiscard]] virtual WindowInput poll() = 0;
    /// Whether the window has somewhere to draw now, which it keeps while it can be seen.
    [[nodiscard]] virtual bool hasSurface() const = 0;
    /// Its size in pixels, for a surface that leaves the swapchain's size to the application.
    [[nodiscard]] virtual VkExtent2D pixels() const = 0;
    /// What an overlay on a `width` x `height` target is drawn for.
    [[nodiscard]] virtual touch::WindowSurface surface(unsigned width, unsigned height) const = 0;
    /// Queues a finger as if it had touched the window.
    virtual void inject(const touch::Finger& finger) = 0;
};

} // namespace opensu::retro
