#include "sdl_pads.hpp"

#include <SDL3/SDL_init.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace opensu::gamepad {
namespace {

constexpr float axisTravel = 32767.0f;

/// SDL's gamepad event for a pad, as SDL_Event's `type` says.
bool isGamepadEvent(Uint32 type) {
    return type == SDL_EVENT_GAMEPAD_ADDED || type == SDL_EVENT_GAMEPAD_REMOVED ||
           type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
           type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
}

std::string idOf(SDL_JoystickID id) {
    return "sdl:" + std::to_string(id);
}

} // namespace

Button SdlPadState::buttonOf(SDL_GamepadButton button) noexcept {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return Button::A;
    case SDL_GAMEPAD_BUTTON_EAST:
        return Button::B;
    case SDL_GAMEPAD_BUTTON_WEST:
        return Button::X;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return Button::Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return Button::L1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return Button::R1;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return Button::L3;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return Button::R3;
    case SDL_GAMEPAD_BUTTON_BACK:
        return Button::Select;
    case SDL_GAMEPAD_BUTTON_START:
        return Button::Start;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return Button::Guide;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return Button::Up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return Button::Down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return Button::Left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return Button::Right;
    default:
        return Button::None;
    }
}

void SdlPadState::button(SDL_GamepadButton button, bool pressed, std::vector<Event>& controls) {
    const Button control = buttonOf(button);
    if (control != Button::None) {
        set(control, pressed, buttonHeld_, controls);
    }
}

void SdlPadState::axis(SDL_GamepadAxis axis, std::int16_t value, std::vector<Event>& controls) {
    const float travel = static_cast<float>(value) / axisTravel;
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        sticks_.leftX = value;
        set(Button::Left, travel < -stickDirectionAt, stickHeld_, controls);
        set(Button::Right, travel > stickDirectionAt, stickHeld_, controls);
        break;
    case SDL_GAMEPAD_AXIS_LEFTY:
        sticks_.leftY = value;
        set(Button::Up, travel < -stickDirectionAt, stickHeld_, controls);
        set(Button::Down, travel > stickDirectionAt, stickHeld_, controls);
        break;
    case SDL_GAMEPAD_AXIS_RIGHTX:
        sticks_.rightX = value;
        break;
    case SDL_GAMEPAD_AXIS_RIGHTY:
        sticks_.rightY = value;
        break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
        const Button trigger = axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? Button::L2 : Button::R2;
        const bool held = triggerHeld_[trigger];
        if (held ? travel <= triggerReleaseAt : travel >= triggerPressAt) {
            set(trigger, !held, triggerHeld_, controls);
        }
        break;
    }
    default:
        break;
    }
}

void SdlPadState::set(Button button, bool on, std::map<Button, bool>& held,
                      std::vector<Event>& controls) {
    bool& was = held[button];
    if (was == on) {
        return;
    }
    was = on;
    Event event;
    event.kind = Event::Kind::Button;
    event.button = button;
    event.pressed = on;
    controls.push_back(event);
}

SdlPads::SdlPads(bool reads) : reads_{reads} {
    if (!reads_) {
        return;
    }
    // Added first: the subsystem announces the pads already connected as it starts.
    if (!SDL_AddEventWatch(watch, this)) {
        throw std::runtime_error{std::string{"cannot watch SDL's events: "} + SDL_GetError()};
    }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        SDL_RemoveEventWatch(watch, this);
        throw std::runtime_error{std::string{"cannot start SDL's gamepads: "} + SDL_GetError()};
    }
}

SdlPads::~SdlPads() {
    if (!reads_) {
        return;
    }
    SDL_RemoveEventWatch(watch, this);
    for (auto& [id, pad] : pads_) {
        SDL_CloseGamepad(pad.gamepad);
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

bool SDLCALL SdlPads::watch(void* self, SDL_Event* event) {
    if (isGamepadEvent(event->type)) {
        auto* pads = static_cast<SdlPads*>(self);
        const std::lock_guard lock{pads->mutex_};
        pads->heard_.push_back(*event);
    }
    return true;
}

std::vector<Event> SdlPads::takeEvents() {
    if (!reads_) {
        return {};
    }
    std::vector<SDL_Event> heard;
    {
        const std::lock_guard lock{mutex_};
        heard = std::exchange(heard_, {});
    }
    std::vector<Event> events;
    for (const SDL_Event& event : heard) {
        hear(event, events);
    }
    return events;
}

void SdlPads::hear(const SDL_Event& event, std::vector<Event>& events) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        const bool added = event.type == SDL_EVENT_GAMEPAD_ADDED;
        const std::optional<PadInfo> pad =
            added ? open(event.gdevice.which) : close(event.gdevice.which);
        if (pad) {
            Event change;
            change.kind = added ? Event::Kind::Connected : Event::Kind::Disconnected;
            change.device = pad->name;
            change.source = pad->id;
            events.push_back(std::move(change));
        }
        return;
    }
    const bool axis = event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
    const SDL_JoystickID id = axis ? event.gaxis.which : event.gbutton.which;
    const std::lock_guard lock{mutex_};
    const auto pad = pads_.find(id);
    if (pad == pads_.end()) {
        return;
    }
    std::vector<Event> controls;
    if (axis) {
        pad->second.state.axis(static_cast<SDL_GamepadAxis>(event.gaxis.axis), event.gaxis.value,
                               controls);
    } else {
        pad->second.state.button(static_cast<SDL_GamepadButton>(event.gbutton.button),
                                 event.gbutton.down, controls);
    }
    for (Event& control : controls) {
        control.source = pad->second.info.id;
        events.push_back(std::move(control));
    }
}

std::optional<PadInfo> SdlPads::open(SDL_JoystickID id) {
    {
        const std::lock_guard lock{mutex_};
        if (pads_.contains(id)) {
            return std::nullopt;
        }
    }
    SDL_Gamepad* gamepad = SDL_OpenGamepad(id);
    if (gamepad == nullptr) {
        return std::nullopt;
    }
    const char* name = SDL_GetGamepadName(gamepad);
    const char* serial = SDL_GetGamepadSerial(gamepad);
    const std::lock_guard lock{mutex_};
    Pad& pad = pads_[id];
    pad.gamepad = gamepad;
    pad.info = PadInfo{.id = idOf(id),
                       .name = name != nullptr ? name : "Gamepad",
                       .uniq = serial != nullptr ? serial : ""};
    return pad.info;
}

std::optional<PadInfo> SdlPads::close(SDL_JoystickID id) {
    const std::lock_guard lock{mutex_};
    const auto pad = pads_.find(id);
    if (pad == pads_.end()) {
        return std::nullopt;
    }
    PadInfo info = pad->second.info;
    SDL_CloseGamepad(pad->second.gamepad);
    pads_.erase(pad);
    return info;
}

std::vector<PadInfo> SdlPads::connected() const {
    const std::lock_guard lock{mutex_};
    std::vector<PadInfo> pads;
    pads.reserve(pads_.size());
    for (const auto& [id, pad] : pads_) {
        pads.push_back(pad.info);
    }
    return pads;
}

std::map<std::string, Sticks> SdlPads::sticks() const {
    const std::lock_guard lock{mutex_};
    std::map<std::string, Sticks> sticks;
    for (const auto& [id, pad] : pads_) {
        sticks.emplace(pad.info.id, pad.state.sticks());
    }
    return sticks;
}

// Nothing is grabbed or forwarded: the game reads the pads itself, so it needs nothing.
std::vector<std::string> SdlPads::hold(ForwardMap /*map*/) {
    return {};
}

void SdlPads::release(Claim /*after*/) {
}

void SdlPads::claim(Claim /*claim*/) {
}

void SdlPads::setBlocked(bool /*blocked*/) {
}

void SdlPads::setChords(ChordTriggers /*chords*/) {
}

std::vector<std::filesystem::path> SdlPads::keepVirtualPads(std::size_t /*count*/) {
    return {};
}

} // namespace opensu::gamepad
