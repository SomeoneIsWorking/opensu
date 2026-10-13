// runner_app — opensu-retro composed: the core and its owners, the window, the pads and keyboard,
// the sound device, and the loop that runs frames at the core's rate on a clock.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace opensu::retro {

/// How a run ends.
enum class RunnerExit : std::uint8_t {
    Quit = 0,
    /// The window or Vulkan could not be set up, or drawing failed.
    PresenterFailed = 1,
    /// The core or game could not be loaded.
    LoadFailed = 2,
    /// The Vulkan validation layer reported errors.
    ValidationErrors = 3,
};

struct RunnerOptions {
    std::filesystem::path core;
    std::filesystem::path rom;
    /// The .srm goes here, named after the ROM.
    std::filesystem::path saves;
    /// The core's system directory (BIOS images).
    std::filesystem::path system;
    /// An unmapped window drawing offscreen, no sound device and no pads: for tests and tools.
    bool hidden{false};
    bool fullscreen{false};
    int width{1280};
    int height{800};
    /// Vulkan's validation layer; its errors end the run as ValidationErrors.
    bool validation{false};
    /// Stop after this many frames.
    std::optional<std::uint64_t> frames;
    /// Hidden only: the last image shown, written here as PNG.
    std::optional<std::filesystem::path> framePng;
};

class RunnerApp {
  public:
    explicit RunnerApp(RunnerOptions options);

    [[nodiscard]] RunnerExit run();

  private:
    RunnerOptions options_;
};

} // namespace opensu::retro
