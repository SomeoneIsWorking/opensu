// runner_app — opensu-retro composed: the core and its owners, the window, the pads and keyboard,
// the sound device, and the loop that runs frames at the core's rate on a clock. Where the shell
// does not hear its input (`shell_handoff`), Guide, Back and the touch pause button bring the
// shell's menu over the game, which goes on under it, the touch controls show while no pad is
// connected and nothing covers the game, and the game waits only while it is out of sight, the
// control channel still served.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "arcade_cabinet.hpp"
#include "core_options.hpp"
#include "display/scale.hpp"
#include "netplay_link.hpp"
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
    /// An unmapped window drawing offscreen, and silent: for tests and tools.
    bool hidden{false};
    /// No sound device and no pads, the window drawn as usual: a hidden shell's game in its
    /// Gamescope.
    bool silent{false};
    bool fullscreen{false};
    int width{1280};
    int height{800};
    /// Where the display's pixels per dp come from.
    display::ScaleSource scale;
    /// Vulkan's validation layer; its errors end the run as ValidationErrors.
    bool validation{false};
    /// Stop after this many frames.
    std::optional<std::uint64_t> frames;
    /// Hidden only: the last image shown, written here as PNG.
    std::optional<std::filesystem::path> framePng;
    /// Hidden only: every frame the core runs, written into this folder as `<frame>.png`.
    std::optional<std::filesystem::path> framePngs;
    /// Where the control channel is announced, or a load failure reported; none when empty.
    std::filesystem::path control;
    /// How the frame is shown: the shell's per-launch settings, else the defaults.
    settings::VideoSettings video;
    /// The librashader a shader preset runs on.
    std::filesystem::path librashader;
    /// Core option values chosen for this launch (a GPU core's renderer among them).
    OptionValues coreOptions;
    /// The ports pads are spread over (`Joypad`); 1 puts every pad on port 0.
    unsigned players{1};
    /// The netplay session this run is one machine of; its pads are then all one player.
    std::optional<NetplaySetup> netplay;
    /// The touch controls' art, where the runner draws its own (`shell_handoff`).
    std::filesystem::path touchArt;
    /// Hidden only: the touch controls shown, at this many pixels per dp whatever `scale` says.
    std::optional<float> touchDp;
    /// Drawn in the shell's window: its surface where it can be, and the shell's input and
    /// menu in place of pads and a window of its own.
    bool embedded{false};
    /// An arcade game, with what its set's listing says of its controls.
    std::optional<CabinetListing> arcade;
};

class RunnerApp {
  public:
    explicit RunnerApp(RunnerOptions options);

    [[nodiscard]] RunnerExit run();

  private:
    RunnerOptions options_;
};

} // namespace opensu::retro
