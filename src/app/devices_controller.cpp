#include "devices_controller.hpp"

#include <algorithm>
#include <memory>

#include "devices_screen.hpp"
#include "gpu2d/ui/screen.hpp"

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
constexpr const char* remapId = "pads.remap";

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

bool DevicesController::isOpen() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const DevicesScreen*>(screen) != nullptr;
    });
}

std::string DevicesController::tab() const {
    if (!isOpen() || !tabs_) {
        return {};
    }
    const auto found = std::ranges::find(*tabs_, chosen_, &ui::SettingsCategory::id);
    return found == tabs_->end() ? std::string{} : found->label;
}

bool DevicesController::showsBluetooth() const {
    return bluetooth_ != nullptr && isOpen() && chosen_ == bluetoothTabId;
}

const host::BluetoothDevice* DevicesController::deviceOf(const std::string& id) const {
    if (bluetooth_ == nullptr || !id.starts_with(devicePrefix)) {
        return nullptr;
    }
    const std::string path = id.substr(std::string_view{devicePrefix}.size());
    const auto& devices = bluetooth_->state().devices;
    const auto found = std::ranges::find(devices, path, &host::BluetoothDevice::path);
    return found == devices.end() ? nullptr : &*found;
}

const std::vector<ui::SettingsCategory>& DevicesController::tabs() {
    if (!tabs_) {
        refreshedAt_ = Clock::now();
        tabs_.emplace();
        if (bluetooth_ != nullptr) {
            tabs_->push_back(bluetoothTab());
        }
        tabs_->push_back(controllersTab());
        tabs_->push_back(audioTab());
        tabs_->push_back(displayTab());
    }
    return *tabs_;
}

ui::SettingsCategory DevicesController::bluetoothTab() const {
    ui::SettingsCategory tab{bluetoothTabId, "Bluetooth", {}};
    const host::BluetoothState& state = bluetooth_->state();
    if (!bluetooth_->known()) {
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
                                      adapter.name + " (" + adapter.address + ")",
                                      adapter.powered));
    if (!bluetooth_->busyWith().empty()) {
        tab.rows.push_back(ui::makeRow("bluetooth.busy", RowKind::Info, bluetooth_->busyWith(),
                                       "BlueZ is working on it", "Please wait"));
    }
    if (!adapter.powered) {
        return tab;
    }
    tab.rows.push_back(ui::makeRow("bluetooth.scan", RowKind::Action, "Scan for devices",
                                   "Put the device in pairing mode first",
                                   adapter.discovering ? "Scanning, press to stop" : "Start"));
    if (state.devices.empty()) {
        tab.rows.push_back(
            ui::makeRow("bluetooth.none", RowKind::Info, "No devices yet", "Scan to find one", ""));
    }
    for (const host::BluetoothDevice& device : state.devices) {
        ui::SettingsRow row = ui::makeRow(devicePrefix + device.path, RowKind::Action, device.name,
                                          deviceNote(device), deviceVerb(device));
        if (device.paired) {
            row.secondary = "Forget";
        }
        tab.rows.push_back(std::move(row));
    }
    return tab;
}

ui::SettingsCategory DevicesController::controllersTab() const {
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
    tab.rows.push_back(ui::makeRow(remapId, RowKind::Action, "Shortcut remapping",
                                   "Buttons and chords, the Guide menu's included", "Open"));
    return tab;
}

ui::SettingsCategory DevicesController::audioTab() const {
    ui::SettingsCategory tab{audioTabId, "Audio output", {}};
    if (const std::string why = outputs_.unavailable(); !why.empty()) {
        tab.rows.push_back(
            ui::makeRow("audio.missing", RowKind::Info, "Audio output", why, "Not available"));
        return tab;
    }
    if (outputs_.sinks().empty()) {
        tab.rows.push_back(ui::makeRow("audio.none", RowKind::Info, "No output found",
                                       "The mixer lists none", ""));
    }
    for (const audio::AudioSink& sink : outputs_.sinks()) {
        tab.rows.push_back(ui::makeRow(sinkPrefix + sink.id,
                                       sink.isDefault ? RowKind::Info : RowKind::Action, sink.name,
                                       {}, sink.isDefault ? "In use" : "Use"));
    }
    tab.rows.push_back(
        ui::makeRow("audio.volume", RowKind::Info, "Volume", {}, "In the Guide menu"));
    return tab;
}

ui::SettingsCategory DevicesController::displayTab() const {
    ui::SettingsCategory tab{displayTabId, "Display", {}};
    tab.rows.push_back(ui::makeRow("output", RowKind::Info, "Output",
                                   "Set by the session that runs openSU", hooks_.output()));
    tab.rows.push_back(ui::makeRow("display.scale", RowKind::Info, "Brightness and interface size",
                                   {}, "In the Guide menu"));
    return tab;
}

void DevicesController::open() {
    if (isOpen()) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    outputs_.refresh();
    if (bluetooth_ != nullptr) {
        bluetooth_->readSoon();
        chosen_ = bluetoothTabId;
    } else {
        chosen_ = controllersTabId;
    }
    refresh();
    navigator_.push(std::make_unique<DevicesScreen>(*this, chrome_));
}

void DevicesController::close() {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const DevicesScreen*>(screen) != nullptr;
    });
    if (found == open.end()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    if (bluetooth_ != nullptr) {
        bluetooth_->stopScan();
    }
    navigator_.closeFrom(**found);
}

void DevicesController::refresh() {
    tabs_.reset();
}

void DevicesController::choose(const std::string& tab) {
    if (tab == chosen_) {
        return;
    }
    chosen_ = tab;
    if (tab == audioTabId) {
        // The outputs are read when the tab is reached, not on a timer.
        outputs_.refresh();
    }
    refresh();
}

void DevicesController::showTabs() {
    while (navigator_.active() && dynamic_cast<const DevicesScreen*>(navigator_.top()) == nullptr) {
        navigator_.pop();
    }
    const auto* screen = dynamic_cast<const DevicesScreen*>(navigator_.top());
    if (screen != nullptr && navigator_.focus(screen->tabId())) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void DevicesController::tick(Clock::time_point now) {
    if (isOpen() && chosen_ == controllersTabId && now - refreshedAt_ >= controllerRefresh) {
        refresh();
    }
}

void DevicesController::controllerChanged() {
    if (isOpen() && chosen_ == controllersTabId) {
        refresh();
    }
}

void DevicesController::refuse(const std::string& reason) {
    if (!reason.empty()) {
        hooks_.say(reason, true);
    }
}

void DevicesController::run(const std::string& id, int /*step*/) {
    if (id == remapId) {
        hooks_.openShortcuts();
    } else if (id.starts_with("bluetooth") || id.starts_with(devicePrefix)) {
        runBluetooth(id);
    } else if (id.starts_with(sinkPrefix)) {
        refuse(outputs_.choose(id.substr(std::string_view{sinkPrefix}.size())));
    }
    refresh();
}

void DevicesController::runBluetooth(const std::string& id) {
    if (bluetooth_ == nullptr) {
        return;
    }
    if (id == "bluetooth.power") {
        const host::BluetoothAdapter* adapter =
            bluetooth_->state().adapter ? &*bluetooth_->state().adapter : nullptr;
        refuse(bluetooth_->setPowered(adapter == nullptr || !adapter->powered));
    } else if (id == "bluetooth.scan") {
        refuse(bluetooth_->toggleScan());
    } else if (const host::BluetoothDevice* device = deviceOf(id)) {
        if (!device->paired) {
            refuse(bluetooth_->pair(*device));
        } else if (device->connected) {
            refuse(bluetooth_->disconnect(*device));
        } else {
            refuse(bluetooth_->connect(*device));
        }
    }
}

void DevicesController::secondary(const std::string& id) {
    const host::BluetoothDevice* device = deviceOf(id);
    if (device == nullptr || !device->paired) {
        return;
    }
    navigator_.push(gpu2d::ui::confirm("Forget " + device->name + "?",
                                       "It has to be paired again before it connects.", "Forget",
                                       [this, id = id] {
                                           if (const host::BluetoothDevice* gone = deviceOf(id)) {
                                               refuse(bluetooth_->forget(*gone));
                                           }
                                           refresh();
                                       }));
}

} // namespace opensu::app
