// runner_touch — the runner's touch controls: the layer laid out for the window, the frame's
// fingers taken while it shows, and what they hold put on the joypad as the pad `touch`. A press
// stays held for frames the core ran, however short the tap: one, or a few for Coin and Start. An
// arcade game (`cabinet`) gets its machine's panel, made again as the core names its controls and
// laid out around the picture.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "arcade_cabinet.hpp"
#include "joypad.hpp"
#include "settings/video_settings.hpp"
#include "touch/touch_layer.hpp"

namespace opensu::retro {

class RunnerTouch {
  public:
    /// The art is read from `art`; what is held goes to `joypad`.
    RunnerTouch(std::filesystem::path art, Joypad& joypad);

    /// The game is an arcade machine whose set lists `listing`.
    void cabinet(const CabinetListing& listing);
    /// The core named its controls.
    void named(const std::vector<InputDescriptor>& descriptors);
    /// The player's size for the controls, a percentage; they are laid out again when it changed.
    void sized(int percent);
    /// The frame is drawn at `picture`, which a machine's panel keeps clear of where it can.
    void showing(const touch::Box& picture);

    /// A frame's window: lays the controls out when `surface` changed, then takes `fingers` while
    /// `shown`, or lets go. Reports whether the pause button went down.
    bool follow(const touch::WindowSurface& surface, bool shown,
                std::span<const touch::Finger> fingers);

    /// The core ran a frame, which counts against the presses still owed one.
    void ran();

    /// Where the controls are, as `GET /touch` gives it.
    [[nodiscard]] std::string places() const;
    /// Whether the controls moved since the last call.
    [[nodiscard]] bool takeMoved() {
        return std::exchange(moved_, false);
    }

    /// The controls as an overlay for the target.
    [[nodiscard]] touch::OverlayImage image() {
        return layer_.image();
    }

  private:
    void place();
    void shape(const std::vector<InputDescriptor>& descriptors, const CabinetListing& listing);

    touch::TouchLayer layer_;
    Joypad& joypad_;
    std::optional<CabinetListing> listing_;
    touch::ArcadePanel panel_;
    touch::WindowSurface laidOut_{};
    touch::Box picture_{};
    int percent_{settings::touchSizeDefault};
    bool moved_{false};
    /// What the joypad was last told.
    std::vector<gamepad::Button> holding_;
    /// Frames each press still stays held for, its finger up or not.
    std::map<gamepad::Button, unsigned> owed_;
};

} // namespace opensu::retro
