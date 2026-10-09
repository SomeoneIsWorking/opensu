// The Devices page's controller: a tab per area, Bluetooth actions through the session with a
// two-step forget, the controllers' live button test, the audio output choice, and the display
// tab's rows.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "devices_controller.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

struct Rig {
    Rig()
        : preferences{settings::Store{std::filesystem::path{OPENSU_TEST_SCRATCH} / "devices" /
                                      "settings.json"},
                      [](const std::string&) {
                      }},
          volume{std::make_unique<test::FakeMixer>(), {}}, outputs{volume.system()},
          bluetooth{radio}, brightness{&light},
          roster{{[this] {
                      return pads;
                  },
                  [](const std::string&) {
                      return std::optional{device::BatteryStatus{90, false}};
                  }}},
          controller{page,
                     {bluetooth, roster, outputs, volume, brightness},
                     sounds,
                     preferences,
                     {[this] {
                          ++layouts;
                      },
                      [this](const std::string& text, bool) {
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
    }
    void openOn(const std::string& tab) {
        controller.open();
        settle();
        while (page.focusedCategory().id != tab) {
            controller.act(Button::Down);
        }
        controller.act(Button::A);
    }
    void focusRow(const std::string& id) {
        for (std::size_t index = 0; index < page.focusedCategory().rows.size(); ++index) {
            if (page.focusedCategory().rows[index].id == id) {
                page.focusRow(index);
                return;
            }
        }
        expect(false, ("a row " + id).c_str());
    }
    [[nodiscard]] const ui::SettingsRow& row(const std::string& id) const {
        for (const ui::SettingsRow& candidate : page.focusedCategory().rows) {
            if (candidate.id == id) {
                return candidate;
            }
        }
        expect(false, ("a row " + id).c_str());
        return page.focusedCategory().rows.front();
    }

    app::Preferences preferences;
    ui::SettingsPage page;
    app::VolumeControl volume;
    app::AudioOutputs outputs;
    test::FakeBluetooth radio;
    app::BluetoothSession bluetooth;
    test::FakeBacklight light;
    app::BrightnessControl brightness;
    std::vector<gamepad::PadInfo> pads;
    app::ControllerRoster roster;
    audio::SoundPlayer sounds;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    int layouts{0};
    app::DevicesController controller;
};

void hasFourTabs() {
    Rig rig;
    rig.controller.open();
    std::vector<std::string> labels;
    for (const ui::SettingsCategory& tab : rig.page.categories()) {
        labels.push_back(tab.label);
    }
    expect(labels ==
               std::vector<std::string>{"Bluetooth", "Controllers", "Audio output", "Display"},
           "the tabs");
    expect(rig.controller.tab() == "Bluetooth" && rig.controller.showsBluetooth(),
           "it opens on Bluetooth, which is then read");
    rig.controller.close();
    expect(rig.controller.tab().empty() && !rig.controller.showsBluetooth(),
           "closed it has no tab");
}

void bluetoothActionsGoThroughTheSession() {
    Rig rig;
    rig.openOn("bluetooth");
    rig.focusRow("bt:/d/buds");
    expect(rig.row("bt:/d/buds").value == "Pair", "an unpaired device offers Pair");
    rig.controller.act(Button::A);
    rig.settle();
    expect(rig.radio.log.front() == "pair /d/buds", "A pairs it");
    expect(rig.row("bt:/d/buds").value == "Disconnect", "and it is connected after");
    rig.focusRow("bt:/d/pad");
    expect(rig.row("bt:/d/pad").value == "Connect", "a paired device offers Connect");
    rig.focusRow("bluetooth.scan");
    rig.controller.act(Button::A);
    rig.settle();
    expect(test::need(rig.radio.state.adapter, "an adapter").discovering,
           "the scan row starts a scan");
    rig.focusRow("bluetooth.power");
    rig.controller.act(Button::A);
    rig.settle();
    expect(!test::need(rig.radio.state.adapter, "an adapter").powered,
           "the power row switches the adapter off");
    expect(rig.page.focusedCategory().rows.size() == 1, "an adapter that is off lists no devices");
}

void forgettingNeedsASecondSelect() {
    Rig rig;
    rig.openOn("bluetooth");
    rig.focusRow("bt:/d/pad");
    expect(rig.controller.forgets(), "Select forgets a paired device");
    rig.controller.act(Button::Select);
    expect(rig.radio.log.empty() && rig.row("bt:/d/pad").note.find("again") != std::string::npos,
           "the first Select asks again");
    rig.controller.act(Button::Select);
    rig.settle();
    expect(rig.radio.log == std::vector<std::string>{"remove /d/pad"}, "the second forgets it");
    rig.focusRow("bt:/d/buds");
    expect(!rig.controller.forgets(), "an unpaired device cannot be forgotten");
}

void aRefusalIsSaid() {
    Rig rig;
    rig.radio.refusal = "org.bluez.Error.Failed";
    rig.openOn("bluetooth");
    rig.focusRow("bt:/d/pad");
    rig.controller.act(Button::A);
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
    event.button = Button::A;
    event.pressed = true;
    event.source = "/dev/input/event5";
    expect(rig.roster.note(event), "the press is noted");
    rig.controller.controllerChanged();
    expect(rig.row(id).value != "Press a button", "the held button shows");
    rig.focusRow("pads.remap");
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"shortcuts"}, "the remapping link opens");
}

void audioListsAndChoosesTheOutput() {
    Rig rig;
    rig.openOn("audio");
    rig.focusRow("sink:2");
    rig.controller.act(Button::A);
    expect(rig.row("sink:2").note == "In use" && rig.row("sink:1").note.empty(),
           "choosing makes the output the one in use");
    rig.focusRow("volume");
    rig.controller.act(Button::Right);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 45, "the volume steps");
}

void displayHasSizeOutputAndBrightness() {
    Rig rig;
    rig.openOn("display");
    expect(rig.row("output").value == "1920 x 1080 at 60 Hz", "the output line");
    rig.focusRow("scale");
    rig.controller.act(Button::Right);
    expect(rig.layouts == 1 && rig.preferences.values().uiScale > 100,
           "the size steps and applies");
    rig.focusRow("brightness");
    rig.controller.chooseLevel(30);
    expect(rig.light.level == 30, "a click sets the brightness");
}

void backGoesToTheTabsThenCloses() {
    Rig rig;
    rig.openOn("audio");
    rig.controller.act(Button::B);
    expect(rig.page.isOpen() && rig.page.zone() == ui::SettingsZone::Categories,
           "B leaves the rows");
    rig.controller.act(Button::B);
    expect(!rig.page.isOpen(), "B closes the page");
}

} // namespace

int main() {
    hasFourTabs();
    bluetoothActionsGoThroughTheSession();
    forgettingNeedsASecondSelect();
    aRefusalIsSaid();
    controllersShowTheLiveButtonTest();
    audioListsAndChoosesTheOutput();
    displayHasSizeOutputAndBrightness();
    backGoesToTheTabsThenCloses();
    std::printf("devices_controller: all checks passed\n");
    return 0;
}
