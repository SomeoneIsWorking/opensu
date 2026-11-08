#include "runner_touch.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// The shortest press the core is given.
constexpr unsigned tapFrames = 1;
/// Boards read these slowly: a CPS2 game drops a one-frame coin.
constexpr unsigned coinFrames = 4;

unsigned framesOf(gamepad::Button button) {
    const bool coin = button == gamepad::Button::Select || button == gamepad::Button::Start;
    return coin ? coinFrames : tapFrames;
}

bool holds(const std::vector<gamepad::Button>& buttons, gamepad::Button button) {
    return std::ranges::find(buttons, button) != buttons.end();
}

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
    layer_.offerRotate();
}

void RunnerTouch::shape(const std::vector<InputDescriptor>& descriptors,
                        const CabinetListing& listing) {
    touch::ArcadePanel panel = panelOf(descriptors, listing);
    if (panel == panel_) {
        return;
    }
    panel_ = std::move(panel);
    std::string named;
    for (const touch::ArcadeButton& button : panel_.buttons) {
        const std::string_view limb = button.limb == touch::Limb::Punch  ? " Punch"
                                      : button.limb == touch::Limb::Kick ? " Kick"
                                                                         : "";
        named += (named.empty() ? "" : ", ") + button.label + std::string{limb};
    }
    lucent::info("retro", "arcade panel: {}-way stick, {} buttons ({})", panel_.ways,
                 panel_.buttons.size(), named);
    layer_.setPanel(panel_);
    place();
}

void RunnerTouch::cabinet(const CabinetListing& listing) {
    listing_ = listing;
    shape({}, listing);
}

void RunnerTouch::named(const std::vector<InputDescriptor>& descriptors) {
    if (listing_) {
        shape(descriptors, *listing_);
    }
}

void RunnerTouch::sized(int percent) {
    if (percent != percent_) {
        percent_ = percent;
        layer_.setSize(static_cast<float>(percent) / 100.0F);
        place();
    }
}

void RunnerTouch::showing(const touch::Box& picture) {
    if (listing_ && picture != picture_) {
        picture_ = picture;
        place();
    }
}

void RunnerTouch::place() {
    if (laidOut_.width == 0) {
        return;
    }
    layer_.layout(laidOut_, picture_);
    owed_.clear();
    holding_.clear();
    joypad_.hold("touch", {});
    moved_ = true;
}

std::string RunnerTouch::places() const {
    nlohmann::json list = nlohmann::json::array();
    for (const touch::ControlPlace& place : layer_.places()) {
        list.push_back({{"control", place.button == gamepad::Button::None
                                        ? std::string{"stick"}
                                        : std::string{gamepad::name(place.button)}},
                        {"x", place.x},
                        {"y", place.y},
                        {"radius", place.radius}});
    }
    return list.dump();
}

bool touchWanted(settings::TouchShow show, input::Device last) noexcept {
    switch (show) {
    case settings::TouchShow::Always:
        return true;
    case settings::TouchShow::Never:
        return false;
    case settings::TouchShow::Dynamic:
        return last != input::Device::Pad;
    }
    return true;
}

bool RunnerTouch::follow(const touch::WindowSurface& surface, bool shown,
                         std::span<const touch::Finger> fingers) {
    if (surface.width != laidOut_.width || surface.height != laidOut_.height ||
        surface.dp != laidOut_.dp) {
        laidOut_ = surface;
        place();
    }
    if (shown) {
        for (const touch::Finger& finger : fingers) {
            const std::vector<gamepad::Button> before = layer_.held();
            layer_.finger(finger);
            for (const gamepad::Button button : layer_.held()) {
                if (!holds(before, button)) {
                    owed_[button] = framesOf(button);
                }
            }
        }
    } else {
        owed_.clear();
        layer_.releaseAll();
    }
    std::vector<gamepad::Button> held = layer_.held();
    for (const auto& [button, frames] : owed_) {
        if (!holds(held, button)) {
            held.push_back(button);
        }
    }
    if (held == holding_) {
        return false;
    }
    const bool pausing = holds(holding_, gamepad::Button::Guide);
    rotate_ = rotate_ ||
              (!holds(holding_, gamepad::Button::Rotate) && holds(held, gamepad::Button::Rotate));
    holding_ = std::move(held);
    joypad_.hold("touch", holding_);
    lucent::info("retro", "touch holds {}", buttonList(holding_));
    return !pausing && holds(holding_, gamepad::Button::Guide);
}

void RunnerTouch::ran() {
    std::erase_if(owed_, [](auto& owed) {
        return --owed.second == 0;
    });
}

} // namespace opensu::retro
