// The Devices screen on the interface toolkit, driven as the shell drives it: a tab per area,
// Bluetooth actions through the session with forgetting asked first, the controllers' live button
// test, the audio output choice, and Back from the rows to the tabs, then out.
#include <chrono>
#include <cstdio>
#include <thread>

#include "devices_controller.hpp"
#include "host_fakes.hpp"
#include "support/fake_interface_text.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using test::expect;

struct Rig {
    Rig()
        : volume{std::make_unique<test::FakeMixer>()}, outputs{volume}, bluetooth{radio},
          roster{{[this] {
                      return pads;
                  },
                  [](const std::string&) {
                      return std::optional{device::BatteryStatus{90, false}};
                  }}},
          controller{ui.navigator,
                     chrome,
                     {bluetooth, roster, outputs},
                     sounds,
                     {[this](const std::string& text, bool) {
                          said.push_back(text);
                      },
                      [this] {
                          calls.emplace_back("shortcuts");
                      },
                      [] {
                          return std::string{"1920 x 1080 at 60 Hz"};
                      }}} {
        radio.state = test::pairedPadAndHeadset();
        pads = {{"/dev/input/event5", "Pro Pad", "aa"}};
    }

    /// Lets the session finish what it started and shows the result.
    void settle() {
        for (int attempt = 0; attempt < 2000; ++attempt) {
            static_cast<void>(bluetooth.poll(app::BluetoothSession::Clock::now(), true));
            if (bluetooth.known() && bluetooth.busyWith().empty()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        controller.refresh();
        ui.frame(1.0);
    }
    void press(Action action) {
        ui.navigator.act(action);
        ui.frame(1.0);
    }
    /// Opens the screen and moves down the tabs to `tab`.
    void openOn(const std::string& tab) {
        controller.open();
        ui.frame(1.0);
        settle();
        for (int i = 0; i < 4 && controller.chosen() != tab; ++i) {
            press(Action::Down);
        }
        expect(controller.chosen() == tab, ("the tab " + tab).c_str());
    }
    /// Focuses row `id` and presses `action` on it.
    void on(const std::string& id, Action action) {
        expect(ui.navigator.focus(id), ("a row " + id).c_str());
        ui.frame(1.0);
        press(action);
    }
    [[nodiscard]] ui::SettingsRow row(const std::string& id) {
        for (const ui::SettingsCategory& tab : controller.tabs()) {
            for (const ui::SettingsRow& candidate : tab.rows) {
                if (candidate.id == id) {
                    return candidate;
                }
            }
        }
        expect(false, ("a row " + id).c_str());
        return {};
    }
    [[nodiscard]] const ui::SettingsCategory& shown() {
        for (const ui::SettingsCategory& tab : controller.tabs()) {
            if (tab.id == controller.chosen()) {
                return tab;
            }
        }
        return controller.tabs().front();
    }

    test::InterfaceRig ui;
    ui::Chrome chrome;
    audio::SystemVolume volume;
    app::AudioOutputs outputs;
    test::FakeBluetooth radio;
    app::BluetoothSession bluetooth;
    std::vector<gamepad::PadInfo> pads;
    app::ControllerRoster roster;
    audio::SdlSoundPlayer sounds;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    app::DevicesController controller;
};

void hasFourTabs() {
    Rig rig;
    rig.controller.open();
    rig.ui.frame(1.0);
    std::vector<std::string> labels;
    for (const ui::SettingsCategory& tab : rig.controller.tabs()) {
        labels.push_back(tab.label);
    }
    expect(labels ==
               std::vector<std::string>{"Bluetooth", "Controllers", "Audio output", "Display"},
           "the tabs");
    expect(rig.controller.tab() == "Bluetooth" && rig.controller.showsBluetooth() &&
               rig.ui.navigator.focused() == "tabs/bluetooth",
           "it opens on Bluetooth, which is then read");
    rig.controller.close();
    rig.ui.frame(1.0);
    expect(rig.controller.tab().empty() && !rig.controller.showsBluetooth(),
           "closed it has no tab");
}

void bluetoothActionsGoThroughTheSession() {
    Rig rig;
    rig.openOn("bluetooth");
    expect(rig.row("bt:/d/buds").value == "Pair", "an unpaired device offers Pair");
    rig.on("bt:/d/buds", Action::Accept);
    rig.settle();
    expect(rig.radio.log.front() == "pair /d/buds", "A pairs it");
    expect(rig.row("bt:/d/buds").value == "Disconnect", "and it is connected after");
    expect(rig.row("bt:/d/pad").value == "Connect", "a paired device offers Connect");
    rig.on("bluetooth.scan", Action::Accept);
    rig.settle();
    expect(test::need(rig.radio.state.adapter, "an adapter").discovering,
           "the scan row starts a scan");
    rig.on("bluetooth.power", Action::Accept);
    rig.settle();
    expect(!test::need(rig.radio.state.adapter, "an adapter").powered,
           "the power row switches the adapter off");
    expect(rig.shown().rows.size() == 1, "an adapter that is off lists no devices");
}

void forgettingAsksFirst() {
    Rig rig;
    rig.openOn("bluetooth");
    expect(rig.row("bt:/d/pad").secondary == "Forget" && rig.row("bt:/d/buds").secondary.empty(),
           "only a paired device can be forgotten");
    rig.on("bt:/d/pad", Action::Secondary);
    expect(rig.radio.log.empty() && rig.ui.navigator.top() != nullptr &&
               rig.ui.navigator.top()->id() == "dialog",
           "X asks first");
    rig.press(Action::Right);
    rig.press(Action::Accept);
    rig.settle();
    expect(rig.radio.log == std::vector<std::string>{"remove /d/pad"},
           "confirmed, it is forgotten");
}

void aRefusalIsSaid() {
    Rig rig;
    rig.radio.refusal = "org.bluez.Error.Failed";
    rig.openOn("bluetooth");
    rig.on("bt:/d/pad", Action::Accept);
    rig.settle();
    expect(test::need(rig.bluetooth.takeNotice(), "a notice").error,
           "the session carries the reason to the player");
}

void controllersShowTheLiveButtonTest() {
    Rig rig;
    rig.openOn("controllers");
    const std::string id = "pad:/dev/input/event5";
    expect(rig.row(id).value == "Press a button" &&
               rig.row(id).note.find("90%") != std::string::npos,
           "name, player and battery");
    gamepad::Event event;
    event.kind = gamepad::Event::Kind::Button;
    event.button = gamepad::Button::A;
    event.pressed = true;
    event.source = "/dev/input/event5";
    expect(rig.roster.note(event), "the press is noted");
    rig.controller.controllerChanged();
    expect(rig.row(id).value != "Press a button", "the held button shows");
    rig.on("pads.remap", Action::Accept);
    expect(rig.calls == std::vector<std::string>{"shortcuts"}, "the remapping link opens");
}

void audioChoosesTheOutput() {
    Rig rig;
    rig.openOn("audio");
    rig.on("sink:2", Action::Accept);
    expect(rig.row("sink:2").value == "In use" && rig.row("sink:1").value == "Use",
           "choosing makes the output the one in use");
}

void displayShowsTheOutput() {
    Rig rig;
    rig.openOn("display");
    expect(rig.row("output").value == "1920 x 1080 at 60 Hz", "the output line");
}

void backGoesToTheTabsThenCloses() {
    Rig rig;
    rig.openOn("audio");
    rig.press(Action::Right);
    expect(!rig.ui.navigator.focused().starts_with("tabs/"), "Right reaches the rows");
    rig.press(Action::Back);
    expect(rig.controller.isOpen() && rig.ui.navigator.focused() == "tabs/audio",
           "Back leaves the rows for the tab");
    rig.press(Action::Back);
    expect(!rig.controller.isOpen(), "Back again closes the screen");
}

} // namespace

int main() {
    hasFourTabs();
    bluetoothActionsGoThroughTheSession();
    forgettingAsksFirst();
    aRefusalIsSaid();
    controllersShowTheLiveButtonTest();
    audioChoosesTheOutput();
    displayShowsTheOutput();
    backGoesToTheTabsThenCloses();
    std::printf("devices_controller: all checks passed\n");
    return 0;
}
