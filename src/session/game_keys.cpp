#include "game_keys.hpp"

#include <array>
#include <stdexcept>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>

namespace opensu::session {

std::optional<GameShortcut> ShortcutChord::key(ShortcutKey key, bool pressed) {
    switch (key) {
    case ShortcutKey::LeftShift:
        leftShift_ = pressed;
        return std::nullopt;
    case ShortcutKey::RightShift:
        rightShift_ = pressed;
        return std::nullopt;
    case ShortcutKey::Tab: {
        const bool repeat = pressed && tabDown_;
        tabDown_ = pressed;
        if (pressed && !repeat && (leftShift_ || rightShift_)) {
            return GameShortcut::Guide;
        }
        return std::nullopt;
    }
    case ShortcutKey::Other:
        return std::nullopt;
    }
    return std::nullopt;
}

struct GameKeys::Connection {
    Display* display{};
    int xinputOpcode{};
    KeyCode leftShift{};
    KeyCode rightShift{};
    KeyCode tab{};

    [[nodiscard]] ShortcutKey classify(int keycode) const {
        if (keycode == leftShift) {
            return ShortcutKey::LeftShift;
        }
        if (keycode == rightShift) {
            return ShortcutKey::RightShift;
        }
        if (keycode == tab) {
            return ShortcutKey::Tab;
        }
        return ShortcutKey::Other;
    }
};

GameKeys::GameKeys() : connection_{std::make_unique<Connection>()} {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        throw std::runtime_error{"cannot open the X display for game shortcuts"};
    }
    connection_->display = display;
    int event = 0;
    int error = 0;
    if (XQueryExtension(display, "XInputExtension", &connection_->xinputOpcode, &event, &error) ==
        False) {
        XCloseDisplay(display);
        throw std::runtime_error{"the X display has no XInput extension"};
    }
    int major = 2;
    int minor = 2;
    if (XIQueryVersion(display, &major, &minor) != Success) {
        XCloseDisplay(display);
        throw std::runtime_error{"the X display has no XInput 2.2"};
    }
    connection_->leftShift = XKeysymToKeycode(display, XK_Shift_L);
    connection_->rightShift = XKeysymToKeycode(display, XK_Shift_R);
    connection_->tab = XKeysymToKeycode(display, XK_Tab);

    std::array<unsigned char, XIMaskLen(XI_LASTEVENT)> mask{};
    XISetMask(mask.data(), XI_RawKeyPress);
    XISetMask(mask.data(), XI_RawKeyRelease);
    XIEventMask eventMask{.deviceid = XIAllMasterDevices,
                          .mask_len = static_cast<int>(mask.size()),
                          .mask = mask.data()};
    XISelectEvents(display, DefaultRootWindow(display), &eventMask, 1);
    XFlush(display);
}

GameKeys::~GameKeys() {
    XCloseDisplay(connection_->display);
}

std::vector<GameShortcut> GameKeys::poll() {
    std::vector<GameShortcut> shortcuts;
    Display* display = connection_->display;
    while (XPending(display) > 0) {
        XEvent event{};
        XNextEvent(display, &event);
        XGenericEventCookie& cookie = event.xcookie;
        if (cookie.type != GenericEvent || cookie.extension != connection_->xinputOpcode ||
            XGetEventData(display, &cookie) == False) {
            continue;
        }
        const bool pressed = cookie.evtype == XI_RawKeyPress;
        const bool released = cookie.evtype == XI_RawKeyRelease;
        const int keycode = static_cast<const XIRawEvent*>(cookie.data)->detail;
        XFreeEventData(display, &cookie);
        if (!pressed && !released) {
            continue;
        }
        if (const auto shortcut = chord_.key(connection_->classify(keycode), pressed)) {
            shortcuts.push_back(*shortcut);
        }
    }
    return shortcuts;
}

} // namespace opensu::session
