// volume_control — the volume shortcuts and the on-screen display. Applies the volume actions
// (up, down, mute) to the system mixer through `audio::SystemVolume` and shows every change,
// whoever made it, once. The desktop's or Gamescope's media keys change the mixer on their own;
// openSU sees those only by watching the mixer, so the two never step twice.
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "audio/system_volume.hpp"
#include "config/config.hpp"
#include "input/shortcuts.hpp"

namespace opensu::app {

class VolumeControl {
  public:
    struct Hooks {
        /// Shows the volume on screen.
        std::function<void(const audio::VolumeState&)> show;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    /// The mixer found on this machine: `wpctl`, else `pactl`. With `readOnly` nothing is written
    /// to it, for runs that must not touch the player's volume.
    [[nodiscard]] static std::unique_ptr<audio::VolumeBackend> detect(const config::Config& config,
                                                                      bool readOnly);

    VolumeControl(std::unique_ptr<audio::VolumeBackend> backend, Hooks hooks);

    [[nodiscard]] audio::SystemVolume& system() noexcept {
        return volume_;
    }
    [[nodiscard]] const audio::SystemVolume& system() const noexcept {
        return volume_;
    }

    /// Does a volume action; the others are not its.
    void act(input::Action action);
    /// Watches the mixer for changes made elsewhere.
    void poll(std::chrono::steady_clock::time_point now) {
        volume_.poll(now);
    }

  private:
    audio::SystemVolume volume_;
    Hooks hooks_;
};

} // namespace opensu::app
