#include "game_keys.hpp"

#include <array>
#include <map>
#include <stdexcept>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>

namespace opensu::session {

std::optional<input::Combo> ShortcutChord::key(int key, bool pressed) {
    if (const input::Modifier modifier = input::modifierOf(key);
        modifier != input::Modifier::Neither) {
        (modifier == input::Modifier::Ctrl ? ctrl_ : shift_) = pressed;
        return std::nullopt;
    }
    if (!pressed) {
        down_.erase(key);
        return std::nullopt;
    }
    const bool repeat = !down_.insert(key).second;
    if (repeat) {
        return std::nullopt;
    }
    return input::Combo{key, ctrl_, shift_};
}

struct GameKeys::Connection {
    Display* display{};
    int xinputOpcode{};
    /// Raylib's key for each X keycode that has one.
    std::map<int, int> keys;
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
    std::vector<int> wanted(input::bindableKeys().begin(), input::bindableKeys().end());
    wanted.insert(wanted.end(), input::modifierKeys().begin(), input::modifierKeys().end());
    for (const int key : wanted) {
        const std::string name = input::x11Name(key);
        if (const KeySym keysym = XStringToKeysym(name.c_str()); keysym != NoSymbol) {
            if (const KeyCode code = XKeysymToKeycode(display, keysym); code != 0) {
                connection_->keys[code] = key;
            }
        }
    }

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

std::vector<input::Action> GameKeys::poll(const input::Shortcuts& shortcuts) {
    std::vector<input::Action> actions;
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
        const auto known = connection_->keys.find(keycode);
        if (known == connection_->keys.end()) {
            continue;
        }
        if (const auto combo = chord_.key(known->second, pressed)) {
            if (const auto action = shortcuts.actionFor(*combo);
                action && input::worksInGame(*action)) {
                actions.push_back(*action);
            }
        }
    }
    return actions;
}

} // namespace opensu::session
