#include "remote_touch.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"
#include "touch/panel_text.hpp"

namespace opensu::app {

RemoteTouch::RemoteTouch(SDL_GPUDevice& device, std::filesystem::path art)
    : device_{&device}, layer_{std::move(art)} {
}

void RemoteTouch::panel(const std::string& text) {
    if (text == panel_) {
        return;
    }
    panel_ = text;
    panelChanged_ = true;
    if (text.empty()) {
        layer_.clearPanel();
    } else if (std::optional<touch::ArcadePanel> panel = touch::panelFromText(text)) {
        layer_.setPanel(std::move(*panel));
    } else {
        lucent::warn("remote", "the host's arcade panel cannot be read");
    }
}

std::vector<gamepad::Event> RemoteTouch::follow(bool shown, const touch::WindowSurface& surface,
                                                const touch::Box& picture,
                                                std::span<const touch::Finger> fingers) {
    std::vector<gamepad::Event> events;
    shown_ = shown;
    if (!shown) {
        layer_.releaseAll();
        noteHeld(events);
        texture_.reset();
        return events;
    }
    const bool moved = picture.x != laidAround_.x || picture.y != laidAround_.y ||
                       picture.width != laidAround_.width || picture.height != laidAround_.height;
    if (surface.width != laidOut_.width || surface.height != laidOut_.height ||
        surface.dp != laidOut_.dp || moved || panelChanged_) {
        layer_.layout(surface, picture);
        laidOut_ = surface;
        laidAround_ = picture;
        panelChanged_ = false;
        noteHeld(events);
    }
    for (const touch::Finger& finger : fingers) {
        if (layer_.finger(finger)) {
            noteHeld(events);
        }
    }
    return events;
}

void RemoteTouch::noteHeld(std::vector<gamepad::Event>& events) {
    const std::vector<gamepad::Button> held = layer_.held();
    const auto note = [&events](gamepad::Button button, bool pressed) {
        events.push_back(gamepad::Event{.button = button, .pressed = pressed});
    };
    for (const gamepad::Button button : held_) {
        if (std::ranges::find(held, button) == held.end()) {
            note(button, false);
        }
    }
    for (const gamepad::Button button : held) {
        if (std::ranges::find(held_, button) == held_.end()) {
            note(button, true);
        }
    }
    held_ = held;
}

std::optional<gpu2d::TextureView> RemoteTouch::view() {
    if (!shown_) {
        return std::nullopt;
    }
    const touch::OverlayImage image = layer_.image();
    if (image.changed || !texture_) {
        texture_.emplace(*device_, image.rgba, static_cast<int>(image.width),
                         static_cast<int>(image.height), gpu2d::Filter::Nearest);
    }
    return texture_->view();
}

} // namespace opensu::app
