// sdl_runner_window — the runner's own SDL window: its events, and a Vulkan surface where it is
// shown. Hidden, it is unmapped and carries the events of a test run.
#pragma once

#include <SDL3/SDL_video.h>

#include "runner_window.hpp"

namespace opensu::retro {

class SdlRunnerWindow final : public RunnerWindow {
  public:
    struct Options {
        bool hidden{false};
        bool fullscreen{false};
        int width{1280};
        int height{800};
    };

    /// Opens it; throws std::runtime_error when SDL's video or the window cannot start.
    explicit SdlRunnerWindow(const Options& options);
    ~SdlRunnerWindow() override;

    [[nodiscard]] std::span<const char* const> surfaceExtensions() const override;
    [[nodiscard]] VkSurfaceKHR openSurface(VkInstance instance) override;
    [[nodiscard]] VkSurfaceKHR makeSurface(VkInstance instance) override;
    void destroySurface(VkInstance instance, VkSurfaceKHR surface) override;

    [[nodiscard]] WindowInput poll() override;
    [[nodiscard]] bool hasSurface() const override;
    [[nodiscard]] VkExtent2D pixels() const override;
    [[nodiscard]] touch::WindowSurface surface(unsigned width, unsigned height) const override;
    void inject(const touch::Finger& finger) override;

  private:
    bool hidden_;
    SDL_Window* window_{nullptr};
};

} // namespace opensu::retro
