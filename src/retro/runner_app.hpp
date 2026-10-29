// runner_app — opensu-retro composed: the core and its owners, the window, the pads and keyboard,
// the sound device, and the loop that runs frames at the core's rate on a clock. Where the shell
// cannot draw over it (`shell_handoff`), Guide, Back and the touch pause button bring the shell's
// menu forward, the touch controls show while no pad is connected, and the game waits while the
// runner is in the background, the control channel still served.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "core_options.hpp"
#include "settings/video_settings.hpp"

namespace opensu::retro {

/// How a run ends.
enum class RunnerExit : std::uint8_t {
    Quit = 0,
    /// The window, Vulkan or the shader preset could not be set up, or drawing failed.
    PresenterFailed = 1,
    /// The core, the game or its save could not be loaded.
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
    /// Where the control channel is announced, or a load failure reported; none when empty.
    std::filesystem::path control;
    /// How the frame is shown: the shell's per-launch settings, else the defaults.
    settings::VideoSettings video;
    /// The librashader a shader preset runs on.
    std::filesystem::path librashader;
    /// Core option values chosen for this launch (a GPU core's renderer among them).
    OptionValues coreOptions;
    /// The touch controls' art, where the runner draws its own (`shell_handoff`).
    std::filesystem::path touchArt;
};

class RunnerApp {
  public:
    explicit RunnerApp(RunnerOptions options);

    [[nodiscard]] RunnerExit run();

  private:
    RunnerOptions options_;
};

} // namespace opensu::retro
