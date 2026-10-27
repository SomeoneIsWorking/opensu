// devices_controller — what the Devices screen holds and does. Four tabs: Bluetooth (the adapter,
// a scan, and each device to pair, connect, disconnect or forget), Controllers (the connected pads
// with player order, battery and a live button test, and the way to the shortcut remapping), Audio
// output (the outputs to choose between) and Display (the output's size). Volume, brightness and
// the interface size are the Guide menu's. Everything it changes goes through the owner of that
// thing: `BluetoothSession` and `AudioOutputs`.
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "audio_outputs.hpp"
#include "bluetooth_session.hpp"
#include "controller_roster.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class DevicesController final : public ui::RowActions {
  public:
    using Clock = std::chrono::steady_clock;

    /// How often the controllers' batteries are looked at while the tab shows.
    static constexpr std::chrono::seconds controllerRefresh{2};

    /// What the screen asks of the shell app.
    struct Hooks {
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
    };

    /// The screen goes on `navigator`'s stack, under `chrome`.
    DevicesController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome, Services services,
                      audio::SdlSoundPlayer& sounds, Hooks hooks)
        : navigator_{navigator}, chrome_{chrome}, bluetooth_{services.bluetooth},
          controllers_{services.controllers}, outputs_{services.outputs}, sounds_{sounds},
          hooks_{std::move(hooks)} {
    }

    /// Opens the screen on its first tab.
    void open();
    /// Closes the screen and what is over it with iiSU's Close sound; a scan it started ends.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// Shows the rows again after something outside the screen changed one.
    void refresh();
    /// The focus goes back to the tabs; the trail's "Devices" level does this.
    void showTabs();
    /// Keeps the controllers' batteries fresh while their tab shows. Main loop only.
    void tick(Clock::time_point now);
    /// A button went down or up on a pad, which the Controllers tab shows.
    void controllerChanged();

    /// The chosen tab's name, or empty when the screen is closed.
    [[nodiscard]] std::string tab() const;
    /// Whether the Bluetooth tab shows, which is when BlueZ is read.
    [[nodiscard]] bool showsBluetooth() const;

    // What the screen asks.
    /// The tabs in their order, as last built.
    [[nodiscard]] const std::vector<ui::SettingsCategory>& tabs();
    /// The tab the screen shows; the screen's tabs set it.
    [[nodiscard]] const std::string& chosen() const noexcept {
        return chosen_;
    }
    void choose(const std::string& tab);
    void run(const std::string& id, int step) override;
    /// X on a paired device: forgets it, asking first.
    void secondary(const std::string& id) override;

  private:
    [[nodiscard]] ui::SettingsCategory bluetoothTab() const;
    [[nodiscard]] ui::SettingsCategory controllersTab() const;
    [[nodiscard]] ui::SettingsCategory audioTab() const;
    [[nodiscard]] ui::SettingsCategory displayTab() const;
    /// The device whose row is `id`, or null.
    [[nodiscard]] const host::BluetoothDevice* deviceOf(const std::string& id) const;
    void runBluetooth(const std::string& id);
    /// Tells the player why a change was refused.
    void refuse(const std::string& reason);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    BluetoothSession& bluetooth_;
    ControllerRoster& controllers_;
    AudioOutputs& outputs_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    std::string chosen_;
    /// The tabs as last built; emptied by `refresh`.
    std::optional<std::vector<ui::SettingsCategory>> tabs_;
    Clock::time_point refreshedAt_{};
};

} // namespace opensu::app
