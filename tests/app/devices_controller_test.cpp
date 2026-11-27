// The Devices screen on the interface toolkit, driven as the shell drives it: a tab per area,
// Bluetooth actions through the session with forgetting asked first, the controllers' live button
// test, a DolphinBar's mode and Bluetooth Wii Remotes, the audio output choice, and Back from the
// rows to the tabs, then out.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "devices_controller.hpp"
#include "host_fakes.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using test::expect;

/// A panel and a TV, the panel in use.
class FakeScreens final : public opensu::host::Screens {
  public:
    std::optional<std::vector<opensu::host::Screen>> read() override {
        return screens;
    }
    std::string use(const std::string& id) override {
        for (opensu::host::Screen& screen : screens) {
            screen.inUse = screen.id == id;
        }
        return {};
    }

    std::vector<opensu::host::Screen> screens{
        {"eDP-1", "Built-in screen", "1920x1080 at 120 Hz", true},
        {"DP-2", "DisplayPort (DP-2)", "1920x1080 at 60 Hz", false}};
};

struct Rig {
    explicit Rig(bool withBluetooth = true)
        : volume{std::make_unique<test::FakeMixer>()}, outputs{volume}, bluetooth{radio},
          roster{{[this] {
                      return pads;
                  },
                  [](const gamepad::PadInfo&) {
                      return std::optional{device::BatteryStatus{90, false}};
                  }}},
          preferences{settings::Store{std::filesystem::path{OPENSU_TEST_SCRATCH} /
                                      "devices_controller" / "settings.json"},
                      [](const std::string&) {
                      }},
          controller{ui.navigator,
                     chrome,
                     {withBluetooth ? &bluetooth : nullptr, roster, outputs, lights, calibration,
                      &screens},
                     sounds,
                     {[this](const std::string& text, bool) {
                          said.push_back(text);
                      },
                      [this] {
                          calls.emplace_back("shortcuts");
                      },
                      [] {
                          return std::string{"1920 x 1080 at 60 Hz"};
                      },
                      [this] {
                          return wiiRemotes;
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
    const ui::Chrome chrome{.width = 1280.0f};
    audio::SystemVolume volume;
    app::AudioOutputs outputs;
    test::FakeBluetooth radio;
    app::BluetoothSession bluetooth;
    std::vector<gamepad::PadInfo> pads;
    device::WiiRemotes wiiRemotes;
    app::ControllerRoster roster;
    audio::SdlSoundPlayer sounds;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    app::Preferences preferences;
    app::RingLightSettings lights{preferences,
                                  std::nullopt,
                                  nullptr,
                                  {.refused =
                                       [](const std::string&) {
                                       },
                                   .allowColours =
                                       [] {
                                       }}};
    std::unique_ptr<gamepad::PadSource> padSource{
        gamepad::openPadSource({.hiddenInputs = std::filesystem::path{OPENSU_TEST_SCRATCH} /
                                                "devices_controller" / "pads"})};
    app::PadCalibrationFlow calibration{ui.navigator,
                                        preferences,
                                        *padSource,
                                        sounds,
                                        [this](const std::string& text, bool) {
                                            said.push_back(text);
                                        },
                                        false};
    FakeScreens screens;
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

void withoutBluetoothItsTabIsLeftOut() {
    Rig rig{false};
    rig.controller.open();
    rig.ui.frame(1.0);
    std::vector<std::string> labels;
    for (const ui::SettingsCategory& tab : rig.controller.tabs()) {
        labels.push_back(tab.label);
    }
    expect(labels == std::vector<std::string>{"Controllers", "Audio output", "Display"},
           "no Bluetooth tab without a Bluetooth service");
    expect(rig.controller.tab() == "Controllers" && !rig.controller.showsBluetooth(),
           "it opens on Controllers");
    rig.controller.close();
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

void padRowsAreTheControllersTabs() {
    Rig rig;
    std::vector<ui::SettingsRow> rows = rig.controller.padRows();
    expect(rows.size() == 1 && rows.front().id == "pad:/dev/input/event5" &&
               rows.front().note == "Player 1 · Battery 90%",
           "each pad with its player and battery");
    rig.pads.clear();
    rows = rig.controller.padRows();
    expect(rows.size() == 1 && rows.front().label == "No controller connected",
           "a row saying no pad is connected");
}

void theBluetoothSwitchStandsAlone() {
    Rig rig;
    std::vector<ui::SettingsRow> rows = rig.controller.bluetoothSwitch();
    expect(rows.size() == 1 && rows.front().id == "bluetooth.checking",
           "checking until BlueZ answers");
    rig.settle();
    rows = rig.controller.bluetoothSwitch();
    expect(rows.size() == 1 && rows.front().id == "bluetooth.power", "then the switch");
    const Rig without{false};
    expect(without.controller.bluetoothSwitch().empty(), "nothing without a Bluetooth service");
}

void aPadIsCalibratedFromItsRow() {
    Rig rig;
    rig.openOn("controllers");
    const std::string id = "calibrate:/dev/input/event5";
    expect(rig.row(id).value == "Calibrate" && rig.row(id).secondary.empty(),
           "an uncalibrated pad offers calibration and nothing to reset");
    rig.on(id, Action::Accept);
    expect(rig.calibration.measuring(), "Calibrate starts measuring the pad");
    rig.calibration.service(app::PadCalibrationFlow::Clock::now());
    expect(!rig.calibration.measuring() && !rig.said.empty() &&
               rig.said.back().find("left before") != std::string::npos,
           "a pad the source no longer reads ends the measuring and says so");

    rig.openOn("controllers");
    rig.on("check:/dev/input/event5", Action::Accept);
    rig.ui.frame(2.0);
    expect(rig.ui.navigator.top() != nullptr && rig.ui.navigator.top()->id() == "pad.check" &&
               !rig.calibration.measuring(),
           "Check shows the pad as games get it, measuring nothing");
}

void wiiRemotesSayWhatWiiGamesPlay() {
    Rig rig;
    rig.openOn("controllers");
    expect(std::ranges::none_of(rig.controller.tabs()[1].rows,
                                [](const ui::SettingsRow& row) {
                                    return row.id.starts_with("wii.");
                                }),
           "no Wii Remote, no line for one");
    rig.wiiRemotes = device::WiiRemotes{.dolphinBars = 1, .barSlots = {}};
    rig.controller.refresh();
    expect(rig.row("wii.bar").label == "DolphinBar (Wii Remote mode)" &&
               rig.row("wii.bar").note == "Wii games play the remotes synced to it",
           "a DolphinBar in Wii Remote mode is named");
    rig.wiiRemotes.barsReachable = false;
    rig.controller.refresh();
    expect(rig.row("wii.bar").note.find("udev") != std::string::npos,
           "one Dolphin cannot open says what it needs");
    rig.wiiRemotes = device::WiiRemotes{.otherModeBars = 1, .bluetoothRemotes = 2, .barSlots = {}};
    rig.controller.refresh();
    expect(rig.row("wii.bar.mode").note.find("mode 4") != std::string::npos,
           "a DolphinBar in another mode is told to switch to mode 4");
    expect(rig.row("wii.bluetooth").label == "2 Wii Remotes", "Bluetooth remotes are counted");
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
    expect(rig.row("screen:eDP-1").value == "In use" && rig.row("screen:DP-2").value == "Use" &&
               rig.row("screen:DP-2").label == "DisplayPort (DP-2)",
           "each screen, the one openSU is on in use");
    rig.press(Action::Right);
    rig.on("screen:DP-2", Action::Accept);
    expect(rig.row("screen:DP-2").value == "In use" && rig.row("screen:eDP-1").value == "Use",
           "Use moves openSU to that screen");
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

int main() try {
    hasFourTabs();
    withoutBluetoothItsTabIsLeftOut();
    bluetoothActionsGoThroughTheSession();
    forgettingAsksFirst();
    aRefusalIsSaid();
    controllersShowTheLiveButtonTest();
    padRowsAreTheControllersTabs();
    theBluetoothSwitchStandsAlone();
    aPadIsCalibratedFromItsRow();
    wiiRemotesSayWhatWiiGamesPlay();
    audioChoosesTheOutput();
    displayShowsTheOutput();
    backGoesToTheTabsThenCloses();
    std::printf("devices_controller: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
