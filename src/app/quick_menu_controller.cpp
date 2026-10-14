#include "quick_menu_controller.hpp"

#include <algorithm>

namespace opensu::app {
namespace {

using ui::RowKind;
using ui::SettingsRow;

constexpr const char* volumeId = "quick.volume";
constexpr const char* muteId = "quick.mute";
constexpr const char* outputId = "quick.output";
constexpr const char* brightnessId = "quick.brightness";
constexpr const char* bluetoothId = "quick.bluetooth";
constexpr const char* devicesId = "quick.devices";
constexpr const char* closeGameId = "quick.closeGame";
constexpr const char* saveStateId = "quick.saveState";
constexpr const char* loadStateId = "quick.loadState";
constexpr const char* resetGameId = "quick.resetGame";

std::string percentOf(int percent) {
    return std::to_string(percent) + "%";
}

} // namespace

std::vector<SettingsRow> QuickMenuController::build() {
    refreshedAt_ = Clock::now();
    std::vector<SettingsRow> rows;
    const audio::SystemVolume& volume = volume_.system();
    if (volume.available()) {
        const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
        rows.push_back(
            ui::makeSlider(volumeId, "Volume", "", ui::SliderRange{state.percent, 0, 100}));
        rows.push_back(ui::makeToggle(muteId, "Mute", "", state.muted));
        if (const audio::AudioSink* sink = outputs_.current()) {
            rows.push_back(ui::makeRow(outputId, RowKind::Choice, "Output", "", sink->name));
        }
    } else {
        rows.push_back(ui::makeRow("quick.volumeMissing", RowKind::Info, "Volume",
                                   volume.unavailable(), "Not available"));
    }
    if (const std::optional<int> level = brightness_.percent()) {
        rows.push_back(
            ui::makeSlider(brightnessId, "Brightness", "", ui::SliderRange{*level, 1, 100}));
    }
    const host::BluetoothState& state = bluetooth_.state();
    if (state.adapter) {
        rows.push_back(ui::makeToggle(bluetoothId, "Bluetooth", bluetooth_.busyWith(),
                                      state.adapter->powered));
    } else {
        rows.push_back(ui::makeRow("quick.bluetoothMissing", RowKind::Info, "Bluetooth",
                                   bluetooth_.known() ? state.unavailable : "Asking BlueZ",
                                   bluetooth_.known() ? "Not available" : "Checking"));
    }
    const std::vector<ControllerStatus> pads = controllers_.controllers(Clock::now());
    if (pads.empty()) {
        rows.push_back(
            ui::makeRow("quick.noControllers", RowKind::Info, "Controllers", "", "None connected"));
    }
    for (const ControllerStatus& pad : pads) {
        rows.push_back(ui::makeRow("quick.pad:" + pad.id, RowKind::Info, pad.name,
                                   "Player " + std::to_string(pad.player),
                                   pad.battery ? percentOf(pad.battery->percent) +
                                                     (pad.battery->charging ? ", charging" : "")
                                               : "No battery"));
    }
    rows.push_back(ui::makeRow(devicesId, RowKind::Action, "Devices", "", "Open"));
    if (hooks_.gameRunning()) {
        addRetroRows(rows);
        rows.push_back(ui::makeRow(closeGameId, RowKind::Action, "Close game", "", ""));
    }
    return rows;
}

void QuickMenuController::addRetroRows(std::vector<SettingsRow>& rows) {
    const std::optional<RetroSession::Status> status = retro_.status();
    if (!status) {
        return;
    }
    rows.push_back(ui::makeRow(saveStateId, RowKind::Action, "Save state", "", ""));
    if (status->hasState) {
        rows.push_back(ui::makeRow(loadStateId, RowKind::Action, "Load state", "",
                                   armed_ == loadStateId ? "Press again to load" : ""));
    } else {
        rows.push_back(ui::makeRow(loadStateId, RowKind::Info, "Load state", "", "No state saved"));
    }
    rows.push_back(ui::makeRow(resetGameId, RowKind::Action, "Reset game", "",
                               armed_ == resetGameId ? "Press again to reset" : ""));
}

void QuickMenuController::runRetro(const SettingsRow& row, bool confirmed) {
    if (row.kind != RowKind::Action) {
        return;
    }
    if (row.id != saveStateId && !confirmed) {
        armed_ = row.id;
        return;
    }
    const RetroSession::Command command = row.id == saveStateId   ? RetroSession::Command::SaveState
                                          : row.id == loadStateId ? RetroSession::Command::LoadState
                                                                  : RetroSession::Command::Reset;
    const RetroSession::Outcome outcome = retro_.perform(command);
    hooks_.say(outcome.text, outcome.error);
}

void QuickMenuController::open() {
    // input-sound.md 3.4 OpenContextMenu: a menu becomes visible (r90.java).
    sounds_.play(audio::Effect::OpenContextMenu);
    armed_.clear();
    outputs_.refresh();
    bluetooth_.readSoon();
    menu_.open(build());
}

void QuickMenuController::close() {
    if (!menu_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: a menu becomes hidden.
    sounds_.play(audio::Effect::Close);
    menu_.close();
}

void QuickMenuController::toggle() {
    if (menu_.isOpen()) {
        close();
    } else {
        open();
    }
}

void QuickMenuController::refresh() {
    if (menu_.isOpen()) {
        menu_.refresh(build());
    }
}

void QuickMenuController::tick(Clock::time_point now) {
    if (menu_.isOpen() && now - refreshedAt_ >= batteryRefresh) {
        refresh();
    }
}

void QuickMenuController::refuse(const std::string& reason) {
    if (!reason.empty()) {
        hooks_.say(reason, true);
    }
}

void QuickMenuController::act(gamepad::Button button) {
    const SettingsRow* focused = menu_.focusedRow();
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (menu_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
            if (!armed_.empty()) {
                armed_.clear();
                refresh();
            }
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (focused != nullptr &&
            (focused->kind == RowKind::Slider || focused->kind == RowKind::Choice)) {
            run(*focused, button == gamepad::Button::Left ? -1 : 1);
        }
        break;
    case gamepad::Button::A:
        if (focused != nullptr) {
            run(*focused, 0);
        }
        break;
    case gamepad::Button::B:
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

void QuickMenuController::chooseLevel(int level) {
    const SettingsRow* focused = menu_.focusedRow();
    if (focused == nullptr || focused->kind != RowKind::Slider) {
        return;
    }
    const int next = std::clamp(level, focused->low, focused->high);
    refuse(focused->id == volumeId ? volume_.system().setPercent(next) : brightness_.set(next));
    refresh();
}

void QuickMenuController::run(const SettingsRow& focused, int step) {
    // The row is a reference into the menu, which a change rebuilds.
    const std::string id = focused.id;
    const bool confirmed = armed_ == id;
    armed_.clear();
    if (id == saveStateId || id == loadStateId || id == resetGameId) {
        if (step == 0) {
            runRetro(focused, confirmed);
        }
    } else if (id == volumeId) {
        if (step != 0) {
            refuse(volume_.system().step(step * audio::volumeStep));
        }
    } else if (id == muteId) {
        if (step == 0) {
            refuse(volume_.system().toggleMute());
        }
    } else if (id == outputId) {
        sounds_.play(audio::Effect::Navigation);
        refuse(outputs_.cycle(step));
    } else if (id == brightnessId) {
        if (step != 0) {
            refuse(brightness_.step(step * brightnessStep));
        }
    } else if (id == bluetoothId) {
        if (step == 0) {
            sounds_.play(audio::Effect::Navigation);
            const host::BluetoothAdapter* adapter =
                bluetooth_.state().adapter ? &*bluetooth_.state().adapter : nullptr;
            refuse(bluetooth_.setPowered(adapter == nullptr || !adapter->powered));
        }
    } else if (id == devicesId) {
        if (step == 0) {
            close();
            hooks_.openDevices();
        }
        return;
    } else if (id == closeGameId) {
        if (step == 0) {
            close();
            hooks_.closeGame();
        }
        return;
    }
    refresh();
}

} // namespace opensu::app
