// game_presentation — the one owner of how Gamescope presents the game: frame generation for the
// game in front, and the running game's render resolution and upscaling. It follows both games
// through the setting layers and hands the platform's output only what changed since its last
// write. Frame generation resolves the game's `framegen.*` keys, with the player's `Lossless.dll`
// found in their Steam libraries; with no game in front, or no DLL, it is off. Scaling resolves
// the game's `scaling.*` keys as the game launches, before it opens its window, so it sees the
// smaller screen from the start; a filter or sharpness change reaches the running game at once, a
// resolution its next launch; when the game is over the screen's size and Gamescope's defaults
// come back. One output per OS, picked in CMakeLists.txt: the Gamescope fork's root atoms inside
// Gamescope on Linux (`game_presentation_gamescope.cpp`); none elsewhere
// (`game_presentation_none.cpp`). The settings are offered wherever the OS has an output, also
// outside Gamescope, where they wait for openSU's Gamescope session.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "game_settings.hpp"
#include "library/game.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/scaling_settings.hpp"
#include "settings/setting_layers.hpp"

namespace opensu::app {

/// Frame generation for the window in front.
struct FrameGenRequest {
    settings::FrameGenSettings settings;
    /// The player's `Lossless.dll`, read in place.
    std::filesystem::path dll;

    bool operator==(const FrameGenRequest&) const = default;
};

/// How a smaller picture fills the screen.
struct Upscale {
    settings::ScalingFilter filter{settings::ScalingFilter::Linear};
    /// Gamescope's sharpness, 0 sharpest to 20 softest.
    std::uint32_t sharpness{0};

    bool operator==(const Upscale&) const = default;
};

/// The compositor that shows the game.
class PresentationOutput {
  public:
    PresentationOutput() = default;
    PresentationOutput(const PresentationOutput&) = delete;
    PresentationOutput& operator=(const PresentationOutput&) = delete;
    PresentationOutput(PresentationOutput&&) = delete;
    PresentationOutput& operator=(PresentationOutput&&) = delete;
    virtual ~PresentationOutput() = default;

    /// Generates frames for the focused window as `request` says.
    virtual void generate(const FrameGenRequest& request) = 0;
    /// Shows real frames only.
    virtual void stopGenerating() = 0;
    /// The size games see as their screen.
    virtual void resize(settings::ScreenSize size) = 0;
    virtual void upscale(const Upscale& upscale) = 0;
};

/// Whether this OS has an output at all, inside Gamescope or not.
[[nodiscard]] bool presentationOnPlatform() noexcept;

/// The platform's output, or null where it has none (another OS, or not inside Gamescope).
/// Throws std::runtime_error when it has one that cannot be opened.
[[nodiscard]] std::unique_ptr<PresentationOutput> openPresentationOutput(bool insideGamescope);

/// A summary line the editor shows over a section, and the note under it.
struct PresentationSummary {
    /// The row's id: `framegen.summary`, `scaling.summary`.
    std::string id;
    std::string line;
    std::string note;
};

class GamePresentation {
  public:
    /// `findDll` looks for the player's `Lossless.dll` in their Steam libraries.
    GamePresentation(const GameSettings& gameSettings, std::unique_ptr<PresentationOutput> output,
                     std::function<std::optional<std::filesystem::path>()> findDll);

    /// Whether frame generation, resolution and upscaling can be set on this OS; their settings
    /// are shown only then.
    [[nodiscard]] static bool supported() noexcept {
        return presentationOnPlatform();
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

    /// The DLL found by the last look; nothing when Lossless Scaling is not installed.
    [[nodiscard]] const std::optional<std::filesystem::path>& dll() const noexcept {
        return dll_;
    }

    /// The game in front (frame generation) and the game launching or running (resolution and
    /// upscaling), each null when there is none. Applies only what changed.
    void follow(const library::Game* inFront, const library::Game* running);
    /// A value of the layers changed: frame generation reaches the game in front, the filter and
    /// sharpness the running game.
    void changed();

    /// The frame generation the values `applies` reads come to; `where` names them in a report.
    [[nodiscard]] PresentationSummary frameGenSummary(const settings::SettingLookup& applies,
                                                      const std::string& where) const;
    /// The resolution and upscaling the values `applies` reads come to.
    [[nodiscard]] PresentationSummary scalingSummary(const settings::SettingLookup& applies,
                                                     const std::string& where) const;

  private:
    /// The game a part follows: its id, title and layers.
    class FollowedGame {
      public:
        /// Takes `game`, null for none; whether it is another game than the one followed.
        bool take(const library::Game* game);
        [[nodiscard]] const library::Game* get() const noexcept {
            return game_ ? &*game_ : nullptr;
        }

      private:
        std::optional<library::Game> game_;
    };

    /// What the output was last given, so only a change is written again.
    template <typename T> class LastWritten {
      public:
        LastWritten() = default;
        /// What the output shows before the first write.
        explicit LastWritten(T start) : value_{std::move(start)} {
        }
        /// Whether `next` is not what the output has; true before the first write without a start.
        [[nodiscard]] bool differs(const T& next) const {
            return !value_ || !(*value_ == next);
        }
        [[nodiscard]] bool written() const noexcept {
            return value_.has_value();
        }
        void wrote(T next) {
            value_ = std::move(next);
        }

      private:
        std::optional<T> value_;
    };

    /// What the game in front resolves to; nothing for off.
    [[nodiscard]] std::optional<FrameGenRequest> wantedFrameGen() const;
    void applyFrameGen();
    /// What the running game resolves to on `screen`; the screen and Gamescope's defaults with no
    /// game. A resize is written only when `resize`.
    void applyScaling(settings::ScreenSize screen, bool resize);
    /// `text` with where it applies, when that is not now.
    [[nodiscard]] std::string noted(std::string text) const;

    const GameSettings& gameSettings_;
    std::unique_ptr<PresentationOutput> output_;
    std::function<std::optional<std::filesystem::path>()> findDll_;
    std::optional<std::filesystem::path> dll_;
    std::optional<settings::ScreenSize> screen_;

    FollowedGame front_;
    LastWritten<std::optional<FrameGenRequest>> frameGen_;

    FollowedGame running_;
    LastWritten<settings::ScreenSize> size_;
    LastWritten<Upscale> upscale_;
};

} // namespace opensu::app
