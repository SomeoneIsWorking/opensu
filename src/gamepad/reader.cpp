#include "reader.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "lucent/log.h"
#include "virtual_pad.hpp"

namespace iideck::gamepad {
namespace {

/// How far the left stick must move before it counts as a direction, chosen
/// above typical stick drift.
constexpr float stickThreshold = 0.5f;

} // namespace

void Reader::setNameFilter(std::vector<std::string> needles) {
    nameFilter_ = std::move(needles);
}

bool Reader::nameAccepted(const char* name) const {
    if (nameFilter_.empty()) {
        return true;
    }
    const std::string device{name != nullptr ? name : ""};
    return std::ranges::any_of(nameFilter_, [&device](const std::string& needle) {
        return !needle.empty() && device.find(needle) != std::string::npos;
    });
}

Button fromGamepadButton(int button) {
    switch (button) {
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:
        return Button::A;
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:
        return Button::B;
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:
        return Button::X;
    case GAMEPAD_BUTTON_RIGHT_FACE_UP:
        return Button::Y;
    // raylib 6.0 dropped the separate shoulder buttons: the triggers carry both
    // the shoulder and the analogue axis.
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1:
        return Button::L1;
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2:
        return Button::L2;
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:
        return Button::R1;
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:
        return Button::R2;
    case GAMEPAD_BUTTON_LEFT_THUMB:
        return Button::L3;
    case GAMEPAD_BUTTON_RIGHT_THUMB:
        return Button::R3;
    case GAMEPAD_BUTTON_MIDDLE_LEFT:
        return Button::Select;
    case GAMEPAD_BUTTON_MIDDLE_RIGHT:
        return Button::Start;
    case GAMEPAD_BUTTON_LEFT_FACE_UP:
        return Button::Up;
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:
        return Button::Right;
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN:
        return Button::Down;
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT:
        return Button::Left;
    default:
        return Button::None;
    }
}

void Reader::poll(std::vector<Event>& events) {
    for (int id = 0; id < maxGamepads; ++id) {
        Slot& slot = slots_[static_cast<std::size_t>(id)];
        // SDL reports any device with buttons as a gamepad, which on a desktop
        // includes the keyboard and its media receiver. A real controller has
        // analogue sticks or triggers, so requiring axes is what separates them.
        // iideck's own virtual pads carry a game's input, never the shell's.
        const bool connected = IsGamepadAvailable(id) && GetGamepadAxisCount(id) >= minAxes &&
                               nameAccepted(GetGamepadName(id)) &&
                               std::string_view{GetGamepadName(id)} != VirtualPad::name;

        if (connected != slot.connected) {
            slot.connected = connected;
            if (connected) {
                slot.held.fill(false);
                events.push_back(
                    Event{.kind = Event::Kind::Connected, .device = GetGamepadName(id)});
                lucent::info("gamepad", "controller connected: {} ({} axes)", GetGamepadName(id),
                             GetGamepadAxisCount(id));
            } else {
                events.push_back(Event{.kind = Event::Kind::Disconnected, .device = "controller"});
            }
            continue;
        }
        if (!connected) {
            continue;
        }

        const char* device = GetGamepadName(id);
        for (int button = 0; button < 32; ++button) {
            const bool down = IsGamepadButtonDown(id, static_cast<GamepadButton>(button));
            if (down == slot.held[static_cast<std::size_t>(button)]) {
                continue;
            }
            slot.held[static_cast<std::size_t>(button)] = down;
            const Button named = fromGamepadButton(button);
            if (named == Button::None) {
                continue;
            }
            events.push_back(Event{
                .kind = Event::Kind::Button, .button = named, .pressed = down, .device = device});
        }

        // The left stick also drives the dpad, so the shell has one control.
        const float lx = GetGamepadAxisMovement(id, GAMEPAD_AXIS_LEFT_X);
        const float ly = GetGamepadAxisMovement(id, GAMEPAD_AXIS_LEFT_Y);
        const auto direction = [](float x, float y) -> Button {
            if (x > stickThreshold) {
                return Button::Right;
            }
            if (x < -stickThreshold) {
                return Button::Left;
            }
            if (y > stickThreshold) {
                return Button::Down;
            }
            if (y < -stickThreshold) {
                return Button::Up;
            }
            return Button::None;
        };
        const Button held = direction(lx, ly);
        const auto slotOf = [](Button button) -> std::size_t {
            switch (button) {
            case Button::Up:
                return 0;
            case Button::Down:
                return 1;
            case Button::Left:
                return 2;
            case Button::Right:
                return 3;
            default:
                return 0;
            }
        };
        if (held == Button::None) {
            for (std::size_t direction = 0; direction < directionHeld_.size(); ++direction) {
                if (directionHeld_[direction]) {
                    directionHeld_[direction] = false;
                    const Button released = direction == 0   ? Button::Up
                                            : direction == 1 ? Button::Down
                                            : direction == 2 ? Button::Left
                                                             : Button::Right;
                    events.push_back(Event{.kind = Event::Kind::Button,
                                           .button = released,
                                           .pressed = false,
                                           .device = device});
                }
            }
        } else {
            const std::size_t index = slotOf(held);
            if (!directionHeld_[index]) {
                directionHeld_[index] = true;
                events.push_back(Event{.kind = Event::Kind::Button,
                                       .button = held,
                                       .pressed = true,
                                       .device = device});
            }
        }

        slot.axisLX = lx;
        slot.axisLY = ly;
        if (lx != 0.0f || ly != 0.0f) {
            events.push_back(
                Event{.kind = Event::Kind::Axis, .axis = "lx", .value = lx, .device = device});
            events.push_back(
                Event{.kind = Event::Kind::Axis, .axis = "ly", .value = ly, .device = device});
        }
    }
}

bool Reader::anyConnected() const {
    return std::ranges::any_of(slots_, [](const Slot& slot) {
        return slot.connected;
    });
}

std::optional<std::string> Reader::primaryName() const {
    for (int id = 0; id < maxGamepads; ++id) {
        if (slots_[static_cast<std::size_t>(id)].connected) {
            return std::string{GetGamepadName(id)};
        }
    }
    return std::nullopt;
}

bool Reader::rumble(float strong, float weak, float seconds) const {
    // raylib exposes one vibration call and no capability query, so the effect
    // goes to the first connected pad and a pad without motors simply ignores
    // it.
    for (int id = 0; id < maxGamepads; ++id) {
        if (!slots_[static_cast<std::size_t>(id)].connected) {
            continue;
        }
        SetGamepadVibration(id, weak, strong, seconds);
        return true;
    }
    return false;
}

} // namespace iideck::gamepad