// quick_menu_controller — the quick menu's rows and buttons: the changes made in place. Volume,
// mute and the output go through the volume owner, brightness through `BrightnessControl`,
// Bluetooth through the shared `BluetoothSession`, and Close game ends the running game as the
// Guide menu does. The controller batteries come from `ControllerRoster`. A game on opensu-retro
// adds Game settings (the settings tabs, `GameSettingsPage`, in the menu's place), Save state,
// Load state and Reset game through `RetroSession`; Load and Reset take a second press.
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "audio_outputs.hpp"
#include "bluetooth_session.hpp"
#include "brightness_control.hpp"
#include "controller_roster.hpp"
#include "gamepad/event.hpp"
#include "retro_session.hpp"
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
        /// Opens the running game's settings tabs in the menu's place.
        std::function<void()> openGameSettings;
    };

    struct Services {
        VolumeControl& volume;
        AudioOutputs& outputs;
        BrightnessControl& brightness;
        BluetoothSession& bluetooth;
        ControllerRoster& controllers;
        RetroSession& retro;
    };

    QuickMenuController(ui::QuickMenu& menu, Services services, audio::SdlSoundPlayer& sounds,
                        Hooks hooks)
        : menu_{menu}, volume_{services.volume}, outputs_{services.outputs},
          brightness_{services.brightness}, bluetooth_{services.bluetooth},
          controllers_{services.controllers}, retro_{services.retro}, sounds_{sounds},
          hooks_{std::move(hooks)} {
    }

    void open();
    /// Opens the menu again on its Game settings row, as the settings tabs close back to it.
    void reopenOnGameSettings();
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
    /// The rows a game on opensu-retro adds; none for any other game.
    void addRetroRows(std::vector<ui::SettingsRow>& rows);
    /// Runs a game row: Save at once, Load and Reset on the second press.
    void runRetro(const ui::SettingsRow& row, bool confirmed);

    ui::QuickMenu& menu_;
    VolumeControl& volume_;
    AudioOutputs& outputs_;
    BrightnessControl& brightness_;
    BluetoothSession& bluetooth_;
    ControllerRoster& controllers_;
    RetroSession& retro_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// The row waiting for its second press; empty when none.
    std::string armed_;
    Clock::time_point refreshedAt_{};
};

} // namespace opensu::app
