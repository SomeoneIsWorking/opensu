#include "host_input.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <linux/input-event-codes.h>

#include "gamepad/remote_pad.hpp"
#include "lucent/log.h"

// Last: Xlib defines None, Button1 and the like as macros.
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

namespace opensu::stream {
namespace {

/// The evdev key of each USB HID keyboard usage (SDL's scancodes are those usages), as the
/// kernel's HID driver maps them.
constexpr std::array<std::pair<std::uint8_t, std::uint16_t>, 106> usageKeys{{
    {0x04, KEY_A},          {0x05, KEY_B},          {0x06, KEY_C},         {0x07, KEY_D},
    {0x08, KEY_E},          {0x09, KEY_F},          {0x0a, KEY_G},         {0x0b, KEY_H},
    {0x0c, KEY_I},          {0x0d, KEY_J},          {0x0e, KEY_K},         {0x0f, KEY_L},
    {0x10, KEY_M},          {0x11, KEY_N},          {0x12, KEY_O},         {0x13, KEY_P},
    {0x14, KEY_Q},          {0x15, KEY_R},          {0x16, KEY_S},         {0x17, KEY_T},
    {0x18, KEY_U},          {0x19, KEY_V},          {0x1a, KEY_W},         {0x1b, KEY_X},
    {0x1c, KEY_Y},          {0x1d, KEY_Z},          {0x1e, KEY_1},         {0x1f, KEY_2},
    {0x20, KEY_3},          {0x21, KEY_4},          {0x22, KEY_5},         {0x23, KEY_6},
    {0x24, KEY_7},          {0x25, KEY_8},          {0x26, KEY_9},         {0x27, KEY_0},
    {0x28, KEY_ENTER},      {0x29, KEY_ESC},        {0x2a, KEY_BACKSPACE}, {0x2b, KEY_TAB},
    {0x2c, KEY_SPACE},      {0x2d, KEY_MINUS},      {0x2e, KEY_EQUAL},     {0x2f, KEY_LEFTBRACE},
    {0x30, KEY_RIGHTBRACE}, {0x31, KEY_BACKSLASH},  {0x32, KEY_BACKSLASH}, {0x33, KEY_SEMICOLON},
    {0x34, KEY_APOSTROPHE}, {0x35, KEY_GRAVE},      {0x36, KEY_COMMA},     {0x37, KEY_DOT},
    {0x38, KEY_SLASH},      {0x39, KEY_CAPSLOCK},   {0x3a, KEY_F1},        {0x3b, KEY_F2},
    {0x3c, KEY_F3},         {0x3d, KEY_F4},         {0x3e, KEY_F5},        {0x3f, KEY_F6},
    {0x40, KEY_F7},         {0x41, KEY_F8},         {0x42, KEY_F9},        {0x43, KEY_F10},
    {0x44, KEY_F11},        {0x45, KEY_F12},        {0x46, KEY_SYSRQ},     {0x47, KEY_SCROLLLOCK},
    {0x48, KEY_PAUSE},      {0x49, KEY_INSERT},     {0x4a, KEY_HOME},      {0x4b, KEY_PAGEUP},
    {0x4c, KEY_DELETE},     {0x4d, KEY_END},        {0x4e, KEY_PAGEDOWN},  {0x4f, KEY_RIGHT},
    {0x50, KEY_LEFT},       {0x51, KEY_DOWN},       {0x52, KEY_UP},        {0x53, KEY_NUMLOCK},
    {0x54, KEY_KPSLASH},    {0x55, KEY_KPASTERISK}, {0x56, KEY_KPMINUS},   {0x57, KEY_KPPLUS},
    {0x58, KEY_KPENTER},    {0x59, KEY_KP1},        {0x5a, KEY_KP2},       {0x5b, KEY_KP3},
    {0x5c, KEY_KP4},        {0x5d, KEY_KP5},        {0x5e, KEY_KP6},       {0x5f, KEY_KP7},
    {0x60, KEY_KP8},        {0x61, KEY_KP9},        {0x62, KEY_KP0},       {0x63, KEY_KPDOT},
    {0x64, KEY_102ND},      {0x65, KEY_COMPOSE},    {0xe0, KEY_LEFTCTRL},  {0xe1, KEY_LEFTSHIFT},
    {0xe2, KEY_LEFTALT},    {0xe3, KEY_LEFTMETA},   {0xe4, KEY_RIGHTCTRL}, {0xe5, KEY_RIGHTSHIFT},
    {0xe6, KEY_RIGHTALT},   {0xe7, KEY_RIGHTMETA},
}};

/// X numbers its keycodes from 8 above evdev's.
constexpr unsigned xKeycodeOffset = 8;
/// The pointer's buttons in `MouseState` order (left, middle, right) as X numbers them, and the
/// wheel's buttons away from and towards the player.
constexpr std::array<unsigned, 3> pointerButtons{1, 2, 3};
constexpr unsigned wheelAway = 4;
constexpr unsigned wheelTowards = 5;
constexpr double pointerScale = 65535.0;

std::optional<unsigned> xKeycodeOf(std::uint8_t usage) {
    const auto found =
        std::ranges::find(usageKeys, usage, &std::pair<std::uint8_t, std::uint16_t>::first);
    if (found == usageKeys.end()) {
        return std::nullopt;
    }
    return found->second + xKeycodeOffset;
}

/// The game's X display, through XTest.
class XDisplay {
  public:
    explicit XDisplay(const std::string& name) : display_{XOpenDisplay(name.c_str())} {
        if (display_ == nullptr) {
            throw std::runtime_error{"cannot open the game's X display " + name};
        }
        int event = 0;
        int error = 0;
        int major = 0;
        int minor = 0;
        if (XTestQueryExtension(display_, &event, &error, &major, &minor) == False) {
            XCloseDisplay(display_);
            throw std::runtime_error{"the game's X display " + name + " has no XTest"};
        }
    }
    ~XDisplay() {
        XCloseDisplay(display_);
    }
    XDisplay(const XDisplay&) = delete;
    XDisplay& operator=(const XDisplay&) = delete;
    XDisplay(XDisplay&&) = delete;
    XDisplay& operator=(XDisplay&&) = delete;

    void key(unsigned keycode, bool down) {
        XTestFakeKeyEvent(display_, keycode, down ? True : False, CurrentTime);
    }
    void button(unsigned button, bool down) {
        XTestFakeButtonEvent(display_, button, down ? True : False, CurrentTime);
    }
    /// Moves the pointer to `x`, `y` across the screen, each 0 to 1.
    void move(double x, double y) {
        const int screen = DefaultScreen(display_);
        const int width = DisplayWidth(display_, screen);
        const int height = DisplayHeight(display_, screen);
        XTestFakeMotionEvent(display_, screen, static_cast<int>(x * (width - 1)),
                             static_cast<int>(y * (height - 1)), CurrentTime);
    }
    void flush() {
        XFlush(display_);
    }

  private:
    Display* display_;
};

} // namespace

class HostInput::Devices {
  public:
    explicit Devices(const std::string& x11Display) : display_{x11Display} {
    }

    gamepad::RemotePad pad;

    void keys(const wire::KeyState& state) {
        for (const std::uint8_t usage : held_) {
            if (std::ranges::find(state.held, usage) == state.held.end()) {
                if (const std::optional<unsigned> keycode = xKeycodeOf(usage)) {
                    display_.key(*keycode, false);
                }
            }
        }
        for (const std::uint8_t usage : state.held) {
            if (std::ranges::find(held_, usage) == held_.end()) {
                if (const std::optional<unsigned> keycode = xKeycodeOf(usage)) {
                    display_.key(*keycode, true);
                }
            }
        }
        held_ = state.held;
        display_.flush();
    }

    void mouse(const wire::MouseState& state) {
        const wire::MouseState before =
            mouse_.value_or(wire::MouseState{.x = 0, .y = 0, .buttons = 0, .wheel = state.wheel});
        if (!mouse_ || before.x != state.x || before.y != state.y) {
            display_.move(state.x / pointerScale, state.y / pointerScale);
        }
        for (std::size_t i = 0; i < pointerButtons.size(); ++i) {
            const bool was = (before.buttons >> i & 1U) != 0;
            const bool is = (state.buttons >> i & 1U) != 0;
            if (was != is) {
                display_.button(pointerButtons[i], is);
            }
        }
        for (std::int32_t notch = before.wheel; notch != state.wheel;) {
            const bool away = state.wheel > notch;
            display_.button(away ? wheelAway : wheelTowards, true);
            display_.button(away ? wheelAway : wheelTowards, false);
            notch += away ? 1 : -1;
        }
        mouse_ = state;
        display_.flush();
    }

    /// Lets go of every key and button held.
    void release() {
        keys(wire::KeyState{});
        if (mouse_) {
            wire::MouseState up = *mouse_;
            up.buttons = 0;
            mouse(up);
        }
        pad.rest();
    }

  private:
    XDisplay display_;
    std::vector<std::uint8_t> held_;
    std::optional<wire::MouseState> mouse_;
};

HostInput::HostInput(const std::string& x11Display)
    : devices_{std::make_unique<Devices>(x11Display)} {
    lucent::info("remote", "the guest's keys and pointer go to X display {}", x11Display);
}

HostInput::~HostInput() {
    devices_->release();
}

void HostInput::pad(const wire::PadState& state) {
    devices_->pad.follow(gamepad::PadSnapshot{.buttons = state.buttons,
                                              .sticks = gamepad::Sticks{.leftX = state.leftX,
                                                                        .leftY = state.leftY,
                                                                        .rightX = state.rightX,
                                                                        .rightY = state.rightY},
                                              .leftTrigger = state.leftTrigger,
                                              .rightTrigger = state.rightTrigger});
}

void HostInput::keys(const wire::KeyState& state) {
    devices_->keys(state);
}

void HostInput::mouse(const wire::MouseState& state) {
    devices_->mouse(state);
}

} // namespace opensu::stream
