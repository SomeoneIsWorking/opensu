// devices_controller — the Devices page's contents and buttons. Four tabs: Bluetooth (the adapter,
// a scan, and each device to pair, connect, disconnect or forget), Controllers (the connected pads
// with player order, battery and a live button test, and the way to the shortcut remapping), Audio
// output (volume, mute and the default output) and Display (interface size, the output's size and
// the brightness where the machine has one). Everything it changes goes through the owner of that
// thing: `BluetoothSession`, `AudioOutputs`/`VolumeControl`, `BrightnessControl`, `Preferences`.
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
#include "preferences.hpp"
#include "ui/settings_page.hpp"
#include "volume_control.hpp"

namespace opensu::app {

class DevicesController {
  public:
    using Clock = std::chrono::steady_clock;

    /// How often the controllers' batteries are looked at while the tab shows.
    static constexpr std::chrono::seconds controllerRefresh{2};

    /// What the page asks of the shell app.
    struct Hooks {
        /// Gives the screen the interface size the preferences now hold.
        std::function<void()> applyLayout;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Opens the Settings screen on its shortcut remapping.
        std::function<void()> openShortcuts;
        /// The output's size and refresh rate as a line ("2560 x 1440 at 60 Hz").
        std::function<std::string()> output;
    };

    /// What the rows act on.
    struct Services {
        BluetoothSession& bluetooth;
        ControllerRoster& controllers;
        AudioOutputs& outputs;
        VolumeControl& volume;
        BrightnessControl& brightness;
    };

    DevicesController(ui::SettingsPage& page, Services services, audio::SoundPlayer& sounds,
                      Preferences& preferences, Hooks hooks)
        : page_{page}, bluetooth_{services.bluetooth}, controllers_{services.controllers},
          outputs_{services.outputs}, volume_{services.volume}, brightness_{services.brightness},
          sounds_{sounds}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// Opens the page on its first tab.
    void open();
    /// Closes the page with iiSU's Close sound; a scan it started ends.
    void close();
    /// A button while the page is up.
    void act(gamepad::Button button);
    /// A click on a slider's track at `level`.
    void chooseLevel(int level);
    /// Shows the rows again after something outside the page changed one.
    void refresh();
    /// The pad goes back to the list of tabs; the trail's "Devices" level does this.
    void showTabs();
    /// Moves the page on: takes the session's finished work and keeps the controllers' batteries
    /// fresh. Main loop only.
    void tick(Clock::time_point now);
    /// A button went down or up on a pad, which the Controllers tab shows.
    void controllerChanged();
    /// A right click on a Bluetooth device: the same as Select.
    void forgetFocused();

    /// The focused tab's name, or empty when the page is closed.
    [[nodiscard]] std::string tab() const;
    /// Whether the Bluetooth tab shows, which is when BlueZ is read.
    [[nodiscard]] bool showsBluetooth() const;
    /// Whether a button now changes the focused row, which the corner prompt says.
    [[nodiscard]] bool changes() const;
    /// Whether Select now forgets the focused Bluetooth device, which the corner prompt says.
    [[nodiscard]] bool forgets() const;

  private:
    [[nodiscard]] std::vector<ui::SettingsCategory> build();
    [[nodiscard]] ui::SettingsCategory bluetoothTab();
    [[nodiscard]] ui::SettingsCategory controllersTab();
    [[nodiscard]] ui::SettingsCategory audioTab();
    [[nodiscard]] ui::SettingsCategory displayTab();
    /// The device whose row is `id`, or null.
    [[nodiscard]] const host::BluetoothDevice* deviceOf(const std::string& id) const;
    /// Acts on `row`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const ui::SettingsRow& row, int step);
    void runBluetooth(const std::string& id, int step);
    void runAudio(const std::string& id, int step);
    void runDisplay(const std::string& id, int step);
    /// Tells the player why a change was refused.
    void refuse(const std::string& reason);

    ui::SettingsPage& page_;
    BluetoothSession& bluetooth_;
    ControllerRoster& controllers_;
    AudioOutputs& outputs_;
    VolumeControl& volume_;
    BrightnessControl& brightness_;
    audio::SoundPlayer& sounds_;
    Preferences& preferences_;
    Hooks hooks_;
    /// The device whose row asked to be forgotten and waits for the second press.
    std::string armed_;
    Clock::time_point refreshedAt_{};
};

} // namespace opensu::app
