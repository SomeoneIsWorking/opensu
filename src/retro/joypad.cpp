#include "joypad.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include <SDL3/SDL_scancode.h>
#include <libretro.h>

namespace opensu::retro {
namespace {

using gamepad::Button;

constexpr std::pair<Button, unsigned> buttonIds[] = {
    {Button::A, RETRO_DEVICE_ID_JOYPAD_B},           {Button::B, RETRO_DEVICE_ID_JOYPAD_A},
    {Button::X, RETRO_DEVICE_ID_JOYPAD_Y},           {Button::Y, RETRO_DEVICE_ID_JOYPAD_X},
    {Button::L1, RETRO_DEVICE_ID_JOYPAD_L},          {Button::R1, RETRO_DEVICE_ID_JOYPAD_R},
    {Button::L2, RETRO_DEVICE_ID_JOYPAD_L2},         {Button::R2, RETRO_DEVICE_ID_JOYPAD_R2},
    {Button::L3, RETRO_DEVICE_ID_JOYPAD_L3},         {Button::R3, RETRO_DEVICE_ID_JOYPAD_R3},
    {Button::Select, RETRO_DEVICE_ID_JOYPAD_SELECT}, {Button::Start, RETRO_DEVICE_ID_JOYPAD_START},
    {Button::Up, RETRO_DEVICE_ID_JOYPAD_UP},         {Button::Down, RETRO_DEVICE_ID_JOYPAD_DOWN},
    {Button::Left, RETRO_DEVICE_ID_JOYPAD_LEFT},     {Button::Right, RETRO_DEVICE_ID_JOYPAD_RIGHT},
};

constexpr std::pair<int, unsigned> keyIds[] = {
    {SDL_SCANCODE_UP, RETRO_DEVICE_ID_JOYPAD_UP},
    {SDL_SCANCODE_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN},
    {SDL_SCANCODE_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT},
    {SDL_SCANCODE_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {SDL_SCANCODE_Z, RETRO_DEVICE_ID_JOYPAD_B},
    {SDL_SCANCODE_X, RETRO_DEVICE_ID_JOYPAD_A},
    {SDL_SCANCODE_A, RETRO_DEVICE_ID_JOYPAD_Y},
    {SDL_SCANCODE_S, RETRO_DEVICE_ID_JOYPAD_X},
    {SDL_SCANCODE_Q, RETRO_DEVICE_ID_JOYPAD_L},
    {SDL_SCANCODE_W, RETRO_DEVICE_ID_JOYPAD_R},
    {SDL_SCANCODE_RETURN, RETRO_DEVICE_ID_JOYPAD_START},
    {SDL_SCANCODE_RSHIFT, RETRO_DEVICE_ID_JOYPAD_SELECT},
};

} // namespace

std::optional<unsigned> Joypad::idOf(Button button) {
    for (const auto& [control, id] : buttonIds) {
        if (control == button) {
            return id;
        }
    }
    return std::nullopt;
}

std::optional<unsigned> Joypad::idOfKey(int scancode) {
    for (const auto& [key, id] : keyIds) {
        if (key == scancode) {
            return id;
        }
    }
    return std::nullopt;
}

void Joypad::apply(const gamepad::Event& event) {
    if (event.kind == gamepad::Event::Kind::Disconnected) {
        pads_.erase(event.source);
        sticks_.erase(event.source);
        return;
    }
    if (event.kind != gamepad::Event::Kind::Button) {
        return;
    }
    const std::optional<unsigned> id = idOf(event.button);
    if (!id) {
        return;
    }
    auto pad = pads_.find(event.source);
    if (pad == pads_.end()) {
        pad = pads_.emplace(event.source, std::array<int, buttons>{}).first;
    }
    int& presses = pad->second[*id];
    presses = event.pressed ? presses + 1 : std::max(presses - 1, 0);
}

void Joypad::sticks(const std::string& source, const gamepad::Sticks& sticks) {
    sticks_.insert_or_assign(source, sticks);
}

void Joypad::key(int scancode, bool down) {
    if (const std::optional<unsigned> id = idOfKey(scancode)) {
        keys_[*id] = down;
    }
}

std::uint16_t Joypad::mask() const {
    std::uint16_t bits = 0;
    for (std::size_t id = 0; id < buttons; ++id) {
        bool held = keys_[id];
        for (const auto& [source, presses] : pads_) {
            held = held || presses[id] > 0;
        }
        if (held) {
            bits = static_cast<std::uint16_t>(bits | (1U << id));
        }
    }
    return bits;
}

std::int16_t Joypad::state(unsigned id) const {
    const std::uint16_t bits = mask();
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        return static_cast<std::int16_t>(bits);
    }
    if (id >= buttons) {
        return 0;
    }
    return static_cast<std::int16_t>((bits >> id) & 1U);
}

std::int16_t Joypad::analog(unsigned index, unsigned id) const {
    // RETRO_DEVICE_INDEX_ANALOG_BUTTON, analogue buttons, is not reported.
    if (index > RETRO_DEVICE_INDEX_ANALOG_RIGHT || id > RETRO_DEVICE_ID_ANALOG_Y) {
        return 0;
    }
    const bool left = index == RETRO_DEVICE_INDEX_ANALOG_LEFT;
    const bool x = id == RETRO_DEVICE_ID_ANALOG_X;
    std::int32_t furthest = 0;
    for (const auto& [source, sticks] : sticks_) {
        const std::int32_t value =
            left ? (x ? sticks.leftX : sticks.leftY) : (x ? sticks.rightX : sticks.rightY);
        if (std::abs(value) > std::abs(furthest)) {
            furthest = value;
        }
    }
    return static_cast<std::int16_t>(std::clamp(furthest, -32768, 32767));
}

} // namespace opensu::retro
