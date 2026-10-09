#include "devices_controller.hpp"

#include <algorithm>

#include "settings/settings.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using ui::SettingsRow;

constexpr const char* bluetoothTabId = "bluetooth";
constexpr const char* controllersTabId = "controllers";
constexpr const char* audioTabId = "audio";
constexpr const char* displayTabId = "display";
constexpr const char* devicePrefix = "bt:";
constexpr const char* padPrefix = "pad:";
constexpr const char* sinkPrefix = "sink:";

std::string joined(const std::vector<std::string>& parts, const char* between) {
    std::string text;
    for (const std::string& part : parts) {
        text += (text.empty() ? "" : between) + part;
    }
    return text;
}

std::string percentOf(int percent) {
    return std::to_string(percent) + "%";
}

/// What a device row reads under its name.
std::string deviceNote(const host::BluetoothDevice& device) {
    std::vector<std::string> parts{device.paired ? "Paired" : "Not paired"};
    if (device.connected) {
        parts.emplace_back("Connected");
    }
    if (device.battery) {
        parts.push_back("Battery " + percentOf(*device.battery));
    }
    return joined(parts, " · ");
}

/// What pressing A on a device does.
std::string deviceVerb(const host::BluetoothDevice& device) {
    if (!device.paired) {
        return "Pair";
    }
    return device.connected ? "Disconnect" : "Connect";
}

std::string heldText(const std::vector<gamepad::Button>& held) {
    std::vector<std::string> names;
    names.reserve(held.size());
    for (const gamepad::Button button : held) {
        names.emplace_back(gamepad::prompt(button));
    }
    return names.empty() ? "Press a button" : joined(names, "  ");
}

} // namespace

std::string DevicesController::tab() const {
    return page_.isOpen() ? page_.focusedCategory().label : std::string{};
}

bool DevicesController::showsBluetooth() const {
    return page_.isOpen() && page_.focusedCategory().id == bluetoothTabId;
}

bool DevicesController::changes() const {
    const SettingsRow* focused = page_.focusedRow();
    return focused != nullptr && focused->kind != RowKind::Info;
}

bool DevicesController::forgets() const {
    const SettingsRow* focused = page_.focusedRow();
    const host::BluetoothDevice* device = focused != nullptr ? deviceOf(focused->id) : nullptr;
    return device != nullptr && device->paired;
}

const host::BluetoothDevice* DevicesController::deviceOf(const std::string& id) const {
    if (!id.starts_with(devicePrefix)) {
        return nullptr;
    }
    const std::string path = id.substr(std::string_view{devicePrefix}.size());
    const auto& devices = bluetooth_.state().devices;
    const auto found = std::ranges::find(devices, path, &host::BluetoothDevice::path);
    return found == devices.end() ? nullptr : &*found;
}

std::vector<ui::SettingsCategory> DevicesController::build() {
    refreshedAt_ = Clock::now();
    return {bluetoothTab(), controllersTab(), audioTab(), displayTab()};
}

ui::SettingsCategory DevicesController::bluetoothTab() {
    ui::SettingsCategory tab{bluetoothTabId, "Bluetooth", {}};
    const host::BluetoothState& state = bluetooth_.state();
    if (!bluetooth_.known()) {
        tab.rows.push_back(ui::makeRow("bluetooth.checking", RowKind::Info, "Bluetooth",
                                       "Asking BlueZ", "Checking"));
        return tab;
    }
    if (!state.adapter) {
        tab.rows.push_back(ui::makeRow("bluetooth.unavailable", RowKind::Info, "Bluetooth",
                                       state.unavailable, "Not available"));
        return tab;
    }
    const host::BluetoothAdapter& adapter = *state.adapter;
    tab.rows.push_back(ui::makeToggle("bluetooth.power", "Bluetooth",
                                      adapter.name + " (" + adapter.address + ")", adapter.powered));
    if (!bluetooth_.busyWith().empty()) {
        tab.rows.push_back(ui::makeRow("bluetooth.busy", RowKind::Info, bluetooth_.busyWith(),
                                       "BlueZ is working on it", "Please wait"));
    }
    if (!adapter.powered) {
        return tab;
    }
    tab.rows.push_back(ui::makeRow("bluetooth.scan", RowKind::Action, "Scan for devices",
                                   "Put the device in pairing mode first",
                                   adapter.discovering ? "Scanning, press to stop" : "Start"));
    if (state.devices.empty()) {
        tab.rows.push_back(ui::makeRow("bluetooth.none", RowKind::Info, "No devices yet",
                                       "Scan to find one", ""));
    }
    for (const host::BluetoothDevice& device : state.devices) {
        const std::string id = devicePrefix + device.path;
        tab.rows.push_back(ui::makeRow(id, RowKind::Action, device.name,
                                       armed_ == device.path ? "Press Select again to forget it"
                                                             : deviceNote(device),
                                       deviceVerb(device)));
    }
    return tab;
}

ui::SettingsCategory DevicesController::controllersTab() {
    ui::SettingsCategory tab{controllersTabId, "Controllers", {}};
    const std::vector<ControllerStatus> pads = controllers_.controllers(Clock::now());
    if (pads.empty()) {
        tab.rows.push_back(ui::makeRow("pads.none", RowKind::Info, "No controller connected",
                                       "Plug one in, or pair it on the Bluetooth tab", ""));
    }
    for (const ControllerStatus& pad : pads) {
        std::vector<std::string> parts{"Player " + std::to_string(pad.player)};
        if (pad.battery) {
            parts.push_back("Battery " + percentOf(pad.battery->percent) +
                            (pad.battery->charging ? ", charging" : ""));
        }
        tab.rows.push_back(ui::makeRow(padPrefix + pad.id, RowKind::Info, pad.name,
                                       joined(parts, " · "), heldText(pad.held)));
    }
    tab.rows.push_back(ui::makeRow("pads.remap", RowKind::Action, "Shortcut remapping",
                                   "Buttons and chords, the quick menu's included", "Open"));
    return tab;
}

ui::SettingsCategory DevicesController::audioTab() {
    ui::SettingsCategory tab{audioTabId, "Audio output", {}};
    const audio::SystemVolume& volume = volume_.system();
    if (!volume.available()) {
        tab.rows.push_back(ui::makeRow("audio.missing", RowKind::Info, "Audio output",
                                       volume.unavailable(), "Not available"));
        return tab;
    }
    const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
    tab.rows.push_back(ui::makeSlider("volume", "Volume", "The output volume of this computer",
                                      ui::SliderRange{state.percent, 0, 100}));
    tab.rows.push_back(ui::makeToggle("mute", "Mute", "Silences the output", state.muted));
    if (outputs_.sinks().empty()) {
        tab.rows.push_back(ui::makeRow("audio.none", RowKind::Info, "No output found",
                                       "The mixer lists none", ""));
    }
    for (const audio::AudioSink& sink : outputs_.sinks()) {
        tab.rows.push_back(ui::makeRow(sinkPrefix + sink.id, RowKind::Action, sink.name,
                                       sink.isDefault ? "In use" : "", sink.isDefault ? "" : "Use"));
    }
    return tab;
}

ui::SettingsCategory DevicesController::displayTab() {
    ui::SettingsCategory tab{displayTabId, "Display", {}};
    tab.rows.push_back(ui::makeRow("scale", RowKind::Choice, "Interface size",
                                   "Scales everything on screen",
                                   percentOf(preferences_.values().uiScale)));
    tab.rows.push_back(ui::makeRow("output", RowKind::Info, "Output",
                                   "Set by the session that runs openSU", hooks_.output()));
    if (const std::optional<int> level = brightness_.percent()) {
        tab.rows.push_back(ui::makeSlider("brightness", "Brightness", "The built-in display",
                                          ui::SliderRange{*level, 1, 100}));
    }
    return tab;
}

void DevicesController::open() {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    armed_.clear();
    outputs_.refresh();
    bluetooth_.readSoon();
    page_.open(build());
}

void DevicesController::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    bluetooth_.stopScan();
    armed_.clear();
    page_.close();
}

void DevicesController::refresh() {
    if (page_.isOpen()) {
        page_.refresh(build());
    }
}

void DevicesController::showTabs() {
    if (page_.leaveRows()) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void DevicesController::tick(Clock::time_point now) {
    if (!page_.isOpen()) {
        return;
    }
    if (page_.focusedCategory().id == controllersTabId && now - refreshedAt_ >= controllerRefresh) {
        refresh();
    }
}

void DevicesController::controllerChanged() {
    if (page_.isOpen() && page_.focusedCategory().id == controllersTabId) {
        refresh();
    }
}

void DevicesController::refuse(const std::string& reason) {
    if (!reason.empty()) {
        hooks_.say(reason, true);
    }
}

void DevicesController::forgetFocused() {
    const SettingsRow* focused = page_.focusedRow();
    const host::BluetoothDevice* device = focused != nullptr ? deviceOf(focused->id) : nullptr;
    if (device == nullptr || !device->paired) {
        return;
    }
    if (armed_ != device->path) {
        armed_ = device->path;
        sounds_.play(audio::Effect::Navigation);
        refresh();
        return;
    }
    armed_.clear();
    refuse(bluetooth_.forget(*device));
    refresh();
}

void DevicesController::act(gamepad::Button button) {
    if (button != gamepad::Button::Select && !armed_.empty()) {
        armed_.clear();
        refresh();
    }
    const bool inRows = page_.zone() == ui::SettingsZone::Rows;
    const SettingsRow* focused = page_.focusedRow();
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: the first press of a D-pad key on a page.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
            if (!inRows && page_.focusedCategory().id == audioTabId) {
                // The outputs are read when the tab is reached, not on a timer.
                outputs_.refresh();
                refresh();
            }
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right: {
        const int step = button == gamepad::Button::Left ? -1 : 1;
        if (!inRows) {
            if (step > 0 && page_.enterRows()) {
                sounds_.play(audio::Effect::Navigation);
            }
        } else if (focused != nullptr &&
                   (focused->kind == RowKind::Choice || focused->kind == RowKind::Slider)) {
            run(*focused, step);
        } else if (step < 0) {
            showTabs();
        }
        break;
    }
    case gamepad::Button::A:
        if (!inRows) {
            if (page_.enterRows()) {
                sounds_.play(audio::Effect::Navigation);
            }
        } else if (focused != nullptr) {
            run(*focused, 0);
        }
        break;
    case gamepad::Button::Select:
        forgetFocused();
        break;
    case gamepad::Button::B:
        if (inRows) {
            showTabs();
        } else {
            close();
        }
        break;
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

void DevicesController::chooseLevel(int level) {
    const SettingsRow* focused = page_.focusedRow();
    if (focused == nullptr || focused->kind != RowKind::Slider) {
        return;
    }
    const int next = std::clamp(level, focused->low, focused->high);
    if (focused->id == "volume") {
        refuse(volume_.system().setPercent(next));
    } else if (focused->id == "brightness") {
        refuse(brightness_.set(next));
    }
    refresh();
}

void DevicesController::run(const SettingsRow& focused, int step) {
    // The row is a reference into the page, which a change rebuilds.
    const std::string id = focused.id;
    if (focused.kind == RowKind::Info) {
        return;
    }
    if (id == "pads.remap") {
        if (step == 0) {
            hooks_.openShortcuts();
        }
    } else if (id.starts_with("bluetooth") || id.starts_with(devicePrefix)) {
        runBluetooth(id, step);
    } else if (id == "volume" || id == "mute" || id.starts_with(sinkPrefix)) {
        runAudio(id, step);
    } else {
        runDisplay(id, step);
    }
    refresh();
}

void DevicesController::runBluetooth(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    sounds_.play(audio::Effect::Navigation);
    if (id == "bluetooth.power") {
        const host::BluetoothAdapter* adapter =
            bluetooth_.state().adapter ? &*bluetooth_.state().adapter : nullptr;
        refuse(bluetooth_.setPowered(adapter == nullptr || !adapter->powered));
    } else if (id == "bluetooth.scan") {
        refuse(bluetooth_.toggleScan());
    } else if (const host::BluetoothDevice* device = deviceOf(id)) {
        if (!device->paired) {
            refuse(bluetooth_.pair(*device));
        } else if (device->connected) {
            refuse(bluetooth_.disconnect(*device));
        } else {
            refuse(bluetooth_.connect(*device));
        }
    }
}

void DevicesController::runAudio(const std::string& id, int step) {
    if (id == "volume") {
        if (step != 0) {
            refuse(volume_.system().step(step * audio::volumeStep));
        }
    } else if (id == "mute") {
        if (step == 0) {
            refuse(volume_.system().toggleMute());
        }
    } else if (step == 0) {
        sounds_.play(audio::Effect::Navigation);
        refuse(outputs_.choose(id.substr(std::string_view{sinkPrefix}.size())));
    }
}

void DevicesController::runDisplay(const std::string& id, int step) {
    if (id == "scale") {
        sounds_.play(audio::Effect::Navigation);
        settings::Settings& values = preferences_.values();
        values.uiScale = settings::steppedUiScale(values.uiScale, step);
        preferences_.save();
        hooks_.applyLayout();
    } else if (id == "brightness" && step != 0) {
        refuse(brightness_.step(step * brightnessStep));
    }
}

} // namespace opensu::app
