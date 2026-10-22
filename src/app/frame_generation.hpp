// frame_generation — the one owner of frame generation for the game in front: when that game
// changes, or one of its frame generation values does, it resolves the game's `framegen.*` keys
// from the setting layers and hands them, with the player's `Lossless.dll` found in their Steam
// libraries, to the platform's output; with no game in front, or no DLL, frame generation is off.
// One output per OS, picked in CMakeLists.txt: the Gamescope fork's GAMESCOPE_FRAMEGEN_* root
// atoms inside Gamescope on Linux (`frame_generation_gamescope.cpp`); none elsewhere
// (`frame_generation_none.cpp`), where the settings are not offered.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "game_settings.hpp"
#include "library/game.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/setting_layers.hpp"

namespace opensu::app {

/// Frame generation for the window in front.
struct FrameGenRequest {
    settings::FrameGenSettings settings;
    /// The player's `Lossless.dll`, read in place.
    std::filesystem::path dll;

    bool operator==(const FrameGenRequest&) const = default;
};

/// Where frame generation is switched: the compositor in front of the game.
class FrameGenOutput {
  public:
    FrameGenOutput() = default;
    FrameGenOutput(const FrameGenOutput&) = delete;
    FrameGenOutput& operator=(const FrameGenOutput&) = delete;
    FrameGenOutput(FrameGenOutput&&) = delete;
    FrameGenOutput& operator=(FrameGenOutput&&) = delete;
    virtual ~FrameGenOutput() = default;

    /// Generates frames for the focused window as `request` says.
    virtual void generate(const FrameGenRequest& request) = 0;
    /// Shows real frames only.
    virtual void off() = 0;
};

/// The platform's output, or null where it has none (another OS, or not inside Gamescope).
/// Throws std::runtime_error when it has one that cannot be opened.
[[nodiscard]] std::unique_ptr<FrameGenOutput> openFrameGenOutput(bool insideGamescope);

class FrameGeneration {
  public:
    /// `findDll` looks for the player's `Lossless.dll` in their Steam libraries.
    FrameGeneration(const GameSettings& gameSettings, std::unique_ptr<FrameGenOutput> output,
                    std::function<std::optional<std::filesystem::path>()> findDll);

    /// Whether frame generation can be set here at all; its settings are shown only then.
    [[nodiscard]] bool available() const noexcept {
        return output_ != nullptr;
    }
    /// The DLL found by the last look; nothing when Lossless Scaling is not installed.
    [[nodiscard]] const std::optional<std::filesystem::path>& dll() const noexcept {
        return dll_;
    }
    /// Looks for the DLL again, as Lossless Scaling may have been installed since; applies a
    /// change.
    void findDll();

    /// The game in front now, or null when none is. Applies only when it changed.
    void follow(const library::Game* inFront);
    /// A frame generation value of the layers changed: applies it to the game in front.
    void changed();

  private:
    /// What the game in front resolves to; nothing for off.
    [[nodiscard]] std::optional<FrameGenRequest> wanted() const;
    void apply();

    const GameSettings& gameSettings_;
    std::unique_ptr<FrameGenOutput> output_;
    std::function<std::optional<std::filesystem::path>()> findDll_;
    std::optional<std::filesystem::path> dll_;
    /// The game in front: its id, title and layers.
    std::optional<library::Game> front_;
    /// What the output was last given; empty before the first write.
    std::optional<std::optional<FrameGenRequest>> written_;
};

} // namespace opensu::app
