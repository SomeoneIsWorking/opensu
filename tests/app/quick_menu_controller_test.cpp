// The quick menu's controller: its rows follow what the machine has, volume and mute reach the
// mixer, the output cycles, Bluetooth toggles, Close game shows only with a game, and B closes.
#include <chrono>
#include <cstdio>
#include <thread>

#include "quick_menu_controller.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

struct Rig {
    explicit Rig(bool gameRunning = false, bool backlight = true)
        : game{gameRunning}, brightness{backlight ? &light : nullptr},
          volume{std::make_unique<test::FakeMixer>(), {}}, outputs{volume.system()},
          bluetooth{radio}, roster{{[] {
                                        return std::vector<gamepad::PadInfo>{
                                            {"/dev/input/event5", "Pro Pad", "aa"}};
                                    },
                                    [](const std::string&) {
                                        return std::optional{device::BatteryStatus{64, true}};
                                    }}},
          controller{menu,
                     {volume, outputs, brightness, bluetooth, roster},
                     sounds,
                     {[this] {
                          return game;
                      },
                      [this] {
                          calls.emplace_back("closeGame");
                      },
                      [this] {
                          calls.emplace_back("devices");
                      },
                      [this](const std::string& text, bool) {
                          said.push_back(text);
                      }}} {
        radio.state = test::pairedPadAndHeadset();
        for (int attempt = 0; attempt < 2000 && !bluetooth.known(); ++attempt) {
            static_cast<void>(bluetooth.poll(app::BluetoothSession::Clock::now(), true));
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    [[nodiscard]] const ui::SettingsRow* row(const std::string& id) const {
        for (const ui::SettingsRow& candidate : menu.rows()) {
            if (candidate.id == id) {
                return &candidate;
            }
        }
        return nullptr;
    }
    void focus(const std::string& id) {
        for (std::size_t index = 0; index < menu.rows().size(); ++index) {
            if (menu.rows()[index].id == id) {
                menu.focusRow(index);
                return;
            }
        }
        expect(false, ("a row " + id).c_str());
    }
    audio::VolumeState mixer() {
        return volume.system().state().value_or(audio::VolumeState{});
    }

    bool game;
    test::FakeBacklight light;
    app::BrightnessControl brightness;
    app::VolumeControl volume;
    app::AudioOutputs outputs;
    test::FakeBluetooth radio;
    app::BluetoothSession bluetooth;
    app::ControllerRoster roster;
    ui::QuickMenu menu;
    audio::SoundPlayer sounds;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    app::QuickMenuController controller;
};

void rowsFollowWhatTheMachineHas() {
    Rig rig;
    rig.controller.open();
    expect(rig.row("quick.volume") && rig.row("quick.mute") && rig.row("quick.output") &&
               rig.row("quick.brightness") && rig.row("quick.bluetooth") && rig.row("quick.devices"),
           "volume, mute, output, brightness, Bluetooth and Devices");
    expect(!rig.row("quick.closeGame"), "no Close game without a game");
    const ui::SettingsRow* pad = rig.row("quick.pad:/dev/input/event5");
    expect(pad && pad->value.find("64%") != std::string::npos, "a controller's battery");
    Rig bare{true, false};
    bare.controller.open();
    expect(!bare.row("quick.brightness"), "no brightness row without a backlight");
    expect(bare.row("quick.closeGame"), "Close game with a game");
}

void volumeMuteAndOutputReachTheMixer() {
    Rig rig;
    rig.controller.open();
    rig.focus("quick.volume");
    rig.controller.act(Button::Right);
    expect(rig.mixer().percent == 45, "Right steps the volume up");
    rig.controller.chooseLevel(80);
    expect(rig.mixer().percent == 80, "a click sets the level");
    rig.focus("quick.mute");
    rig.controller.act(Button::A);
    expect(rig.mixer().muted && rig.row("quick.mute")->on, "A mutes and the row shows it");
    rig.focus("quick.output");
    rig.controller.act(Button::A);
    expect(rig.row("quick.output")->value == "Headphones", "A moves to the next output");
}

void brightnessStepsAndBluetoothToggles() {
    Rig rig;
    rig.controller.open();
    rig.focus("quick.brightness");
    rig.controller.act(Button::Left);
    expect(rig.light.level == 45, "Left lowers the brightness");
    rig.focus("quick.bluetooth");
    rig.controller.act(Button::A);
    for (int attempt = 0; attempt < 2000 && !rig.bluetooth.busyWith().empty(); ++attempt) {
        static_cast<void>(rig.bluetooth.poll(app::BluetoothSession::Clock::now(), true));
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    static_cast<void>(rig.bluetooth.poll(app::BluetoothSession::Clock::now(), true));
    expect(rig.radio.log == std::vector<std::string>{"power 0"}, "A switches Bluetooth off");
}

void devicesAndCloseGameAreActions() {
    Rig rig{true};
    rig.controller.open();
    rig.focus("quick.devices");
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"devices"} && !rig.menu.isOpen(),
           "Devices closes the menu and opens the page");
    rig.controller.open();
    rig.focus("quick.closeGame");
    rig.controller.act(Button::A);
    expect(rig.calls.back() == "closeGame" && !rig.menu.isOpen(), "Close game ends the game");
}

void backClosesAndToggleTogglesAndUpDownMove() {
    Rig rig;
    rig.controller.toggle();
    expect(rig.menu.isOpen() && rig.menu.focus() == 0, "toggle opens on the first row");
    rig.controller.act(Button::Down);
    expect(rig.menu.focus() == 1, "Down moves");
    rig.controller.act(Button::B);
    expect(!rig.menu.isOpen(), "B closes");
    rig.controller.toggle();
    rig.controller.toggle();
    expect(!rig.menu.isOpen(), "toggle closes");
}

} // namespace

int main() {
    rowsFollowWhatTheMachineHas();
    volumeMuteAndOutputReachTheMixer();
    brightnessStepsAndBluetoothToggles();
    devicesAndCloseGameAreActions();
    backClosesAndToggleTogglesAndUpDownMove();
    std::printf("quick_menu_controller: all checks passed\n");
    return 0;
}
