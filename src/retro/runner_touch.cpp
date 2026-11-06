#include "runner_touch.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// `buttons` by their prompts, "nothing" for none.
std::string buttonList(const std::vector<gamepad::Button>& buttons) {
    std::string list;
    for (const gamepad::Button button : buttons) {
        list += (list.empty() ? "" : " ") + std::string{gamepad::prompt(button)};
    }
    return list.empty() ? "nothing" : list;
}

} // namespace

RunnerTouch::RunnerTouch(std::filesystem::path art, Joypad& joypad)
    : layer_{std::move(art)}, joypad_{joypad} {
}

bool RunnerTouch::follow(const touch::WindowSurface& surface, bool shown,
                         std::span<const touch::Finger> fingers) {
    if (surface.width != laidOut_.width || surface.height != laidOut_.height ||
        surface.dp != laidOut_.dp) {
        layer_.layout(surface);
        laidOut_ = surface;
        joypad_.hold("touch", {});
    }
    const bool pausing = layer_.pressed(gamepad::Button::Guide);
    bool changed = false;
    if (shown) {
        std::vector<touch::Finger> taken = std::exchange(liftNext_, {});
        taken.insert(taken.end(), fingers.begin(), fingers.end());
        std::vector<std::uint64_t> wentDown;
        for (const touch::Finger& finger : taken) {
            if (finger.phase == touch::Finger::Phase::Up &&
                std::ranges::find(wentDown, finger.id) != wentDown.end()) {
                liftNext_.push_back(finger);
                continue;
            }
            if (finger.phase == touch::Finger::Phase::Down) {
                wentDown.push_back(finger.id);
            }
            changed = layer_.finger(finger) || changed;
        }
    } else {
        liftNext_.clear();
        changed = layer_.releaseAll();
    }
    if (!changed) {
        return false;
    }
    const std::vector<gamepad::Button> held = layer_.held();
    joypad_.hold("touch", held);
    lucent::info("retro", "touch holds {}", buttonList(held));
    return !pausing && layer_.pressed(gamepad::Button::Guide);
}

} // namespace opensu::retro
