#include "wii_remote_settings.hpp"

#include "lucent/log.h"

namespace opensu::app {
namespace {

constexpr const char* realId = "wii.real";
constexpr const char* pointerId = "wii.pointer";

std::string placeOf(gamepad::SensorBarPlace place) {
    switch (place) {
    case gamepad::SensorBarPlace::Above:
        return "Fitted, sensor bar above the screen";
    case gamepad::SensorBarPlace::Below:
        return "Fitted, sensor bar below the screen";
    case gamepad::SensorBarPlace::Level:
        return "Fitted";
    }
    return "Fitted";
}

} // namespace

WiiRemoteSettings::WiiRemoteSettings(Preferences& preferences, gamepad::PadSource& pads,
                                     std::function<void()> changed)
    : preferences_{preferences}, pads_{pads}, changed_{std::move(changed)} {
    pads_.setWiiPointerRange(saved());
}

gamepad::WiiPointerRange WiiRemoteSettings::saved() const {
    return preferences_.values().wiiPointer.value_or(gamepad::WiiPointerRange{});
}

bool WiiRemoteSettings::realRemotes() const {
    return preferences_.values().realWiiRemotes;
}

std::vector<ui::SettingsRow> WiiRemoteSettings::rows() const {
    std::vector<ui::SettingsRow> rows;
    rows.push_back(ui::makeHeading("Wii Remotes"));
    const bool real = realRemotes();
    rows.push_back(ui::makeChoice(
        realId, "Wii Remotes in Wii games",
        real ? "A DolphinBar in mode 4 or a remote over Bluetooth plays the game"
             : "The game plays its emulated remote on your pad",
        ui::Options{.labels = {"Real when here", "Emulated always"}, .chosen = real ? 0U : 1U}));
    std::string note = "Point a remote at two corners of the screen to fit the pointer to it";
    std::string value = "Not fitted";
    if (corner_ == Corner::TopLeft) {
        note = "Point at the screen's top-left corner and press A; A here again stops";
        value = "Top-left corner";
    } else if (corner_ == Corner::BottomRight) {
        note = "Point at the screen's bottom-right corner and press A";
        value = "Bottom-right corner";
    } else if (const std::optional<gamepad::WiiPointerRange>& range =
                   preferences_.values().wiiPointer) {
        value = placeOf(range->barPlace());
    }
    rows.push_back(ui::makeRow(pointerId, ui::RowKind::Action, "Wii Remote pointer", note, value));
    return rows;
}

bool WiiRemoteSettings::owns(const std::string& id) {
    return id == realId || id == pointerId;
}

void WiiRemoteSettings::run(const std::string& id, int /*step*/) {
    if (id == realId) {
        settings::Settings& values = preferences_.values();
        values.realWiiRemotes = !values.realWiiRemotes;
        preferences_.save();
    } else if (fitting()) {
        stop();
    } else {
        corner_ = Corner::TopLeft;
        aims_.clear();
        // The camera's whole view, so the aims read are the camera's own.
        pads_.setWiiPointerRange(gamepad::WiiPointerRange{});
        lucent::info("gamepad", "fitting the Wii Remote pointer: point at the top-left corner");
    }
    changed_();
}

void WiiRemoteSettings::stop() {
    corner_ = Corner::None;
    aims_.clear();
    pads_.setWiiPointerRange(saved());
}

bool WiiRemoteSettings::take(const gamepad::Event& event) {
    if (!fitting()) {
        return false;
    }
    if (event.kind == gamepad::Event::Kind::Pointer) {
        if (event.onScreen) {
            aims_[event.source] = {event.x, event.y};
        } else {
            aims_.erase(event.source);
        }
        return true;
    }
    if (event.kind != gamepad::Event::Kind::PointerButton) {
        return false;
    }
    const auto aim = aims_.find(event.source);
    if (!event.pressed || aim == aims_.end()) {
        return true;
    }
    if (corner_ == Corner::TopLeft) {
        topLeft_ = aim->second;
        corner_ = Corner::BottomRight;
        lucent::info("gamepad", "Wii Remote pointer: top-left corner at {:.3f}, {:.3f}",
                     topLeft_.first, topLeft_.second);
        changed_();
        return true;
    }
    const std::optional<gamepad::WiiPointerRange> range = gamepad::WiiPointerRange::fromCorners(
        topLeft_.first, topLeft_.second, aim->second.first, aim->second.second);
    if (!range) {
        // The same spot twice: the bottom-right corner is asked again.
        lucent::info("gamepad", "Wii Remote pointer: the corners are too close; point again");
        return true;
    }
    preferences_.values().wiiPointer = *range;
    preferences_.save();
    lucent::info("gamepad",
                 "Wii Remote pointer fitted: across {:.3f} to {:.3f}, down {:.3f} to {:.3f}, {}",
                 range->left, range->right, range->top, range->bottom, placeOf(range->barPlace()));
    stop();
    changed_();
    return true;
}

} // namespace opensu::app
