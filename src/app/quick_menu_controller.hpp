// quick_menu_controller — the quick menu's rows and buttons: the changes made in place. Volume,
// mute and the output go through the volume owner, brightness through `BrightnessControl`,
// Bluetooth through the shared `BluetoothSession`, and Close game ends the running game as the
// Guide menu does. The controller batteries come from `ControllerRoster`.
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "audio/sound_player.hpp"
#include "audio_outputs.hpp"
#include "bluetooth_session.hpp"
#include "brightness_control.hpp"
#include "controller_roster.hpp"
#include "gamepad/event.hpp"
#include "ui/quick_menu.hpp"
#include "volume_control.hpp"

namespace opensu::app {

class QuickMenuController {
  public:
    using Clock = std::chrono::steady_clock;

    /// How often the controllers' batteries are looked at while the menu is up.
    static constexpr std::chrono::seconds batteryRefresh{2};

    struct Hooks {
        /// Whether a game is running, which gives the menu its Close game row.
        std::function<bool()> gameRunning;
        std::function<void()> closeGame;
        std::function<void()> openDevices;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    struct Services {
        VolumeControl& volume;
        AudioOutputs& outputs;
        BrightnessControl& brightness;
        BluetoothSession& bluetooth;
        ControllerRoster& controllers;
    };

    QuickMenuController(ui::QuickMenu& menu, Services services, audio::SoundPlayer& sounds,
                        Hooks hooks)
        : menu_{menu}, volume_{services.volume}, outputs_{services.outputs},
          brightness_{services.brightness}, bluetooth_{services.bluetooth},
          controllers_{services.controllers}, sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    void open();
    /// Closes the menu with iiSU's Close sound.
    void close();
    void toggle();
    /// A button while the menu is up.
    void act(gamepad::Button button);
    /// A click on a slider's track at `level`.
    void chooseLevel(int level);
    /// Shows the rows again after something outside the menu changed one.
    void refresh();
    /// Keeps the controllers' batteries fresh. Main loop only.
    void tick(Clock::time_point now);

  private:
    [[nodiscard]] std::vector<ui::SettingsRow> build();
    /// Acts on `row`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const ui::SettingsRow& row, int step);
    void refuse(const std::string& reason);

    ui::QuickMenu& menu_;
    VolumeControl& volume_;
    AudioOutputs& outputs_;
    BrightnessControl& brightness_;
    BluetoothSession& bluetooth_;
    ControllerRoster& controllers_;
    audio::SoundPlayer& sounds_;
    Hooks hooks_;
    Clock::time_point refreshedAt_{};
};

} // namespace opensu::app
