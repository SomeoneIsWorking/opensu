// game_scaling — the one owner of the running game's render resolution and upscaling: as a game
// launches it resolves the game's `scaling.*` keys from the setting layers and hands them to the
// platform's output, before the game opens its window, so it sees the smaller screen from the
// start; a filter or sharpness change reaches the running game at once, a resolution change at its
// next launch; when the game is over the screen's own size and Gamescope's defaults come back.
// One output per OS, picked in CMakeLists.txt: Gamescope's root atoms inside Gamescope on Linux
// (`game_scaling_gamescope.cpp`); none elsewhere (`game_scaling_none.cpp`). The settings are
// offered wherever the OS has an output, also outside Gamescope, where they wait for openSU's
// Gamescope session.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "game_settings.hpp"
#include "library/game.hpp"
#include "settings/scaling_settings.hpp"

namespace opensu::app {

/// Where the game's resolution and upscaling are set: the compositor that shows it.
class ScalingOutput {
  public:
    ScalingOutput() = default;
    ScalingOutput(const ScalingOutput&) = delete;
    ScalingOutput& operator=(const ScalingOutput&) = delete;
    ScalingOutput(ScalingOutput&&) = delete;
    ScalingOutput& operator=(ScalingOutput&&) = delete;
    virtual ~ScalingOutput() = default;

    /// The size games see as their screen.
    virtual void resize(settings::ScreenSize size) = 0;
    /// How a smaller picture fills the screen.
    virtual void upscale(settings::ScalingFilter filter, std::uint32_t gamescopeSharpness) = 0;
};

/// Whether this OS has an output at all, inside Gamescope or not.
[[nodiscard]] bool scalingOnPlatform() noexcept;

/// The platform's output, or null where it has none (another OS, or not inside Gamescope).
/// Throws std::runtime_error when it has one that cannot be opened.
[[nodiscard]] std::unique_ptr<ScalingOutput> openScalingOutput(bool insideGamescope);

class GameScaling {
  public:
    GameScaling(const GameSettings& gameSettings, std::unique_ptr<ScalingOutput> output);

    /// Whether resolution and upscaling can be set on this OS; their settings are shown only then.
    [[nodiscard]] static bool supported() noexcept {
        return scalingOnPlatform();
    }
    /// Whether they work now: openSU runs inside its Gamescope session.
    [[nodiscard]] bool active() const noexcept {
        return output_ != nullptr;
    }
    /// The screen's own size, which a resolution is a share of; nothing before it is known.
    [[nodiscard]] const std::optional<settings::ScreenSize>& screen() const noexcept {
        return screen_;
    }
    /// The screen's own size, read before any game made it smaller.
    void setScreen(settings::ScreenSize screen);

    /// The game launching or running now, or null when none is. Applies only when it changed.
    void follow(const library::Game* running);
    /// A scaling value of the layers changed: the filter and sharpness reach the running game.
    void changed();

  private:
    /// What is applied now, by part.
    struct Applied {
        settings::ScreenSize size;
        settings::ScalingFilter filter{settings::ScalingFilter::Linear};
        std::uint32_t sharpness{0};

        bool operator==(const Applied&) const = default;
    };

    /// What `game`'s layers ask for on `screen`; the screen and Gamescope's defaults with no game.
    [[nodiscard]] Applied wanted(const library::Game* game, settings::ScreenSize screen) const;
    /// Hands `next` to the output, each part only when it differs from what it has.
    void apply(const Applied& next);

    const GameSettings& gameSettings_;
    std::unique_ptr<ScalingOutput> output_;
    std::optional<settings::ScreenSize> screen_;
    /// The game launching or running.
    std::optional<library::Game> running_;
    /// What the output was last given; Gamescope's own start before the first write.
    Applied applied_;
};

} // namespace opensu::app
