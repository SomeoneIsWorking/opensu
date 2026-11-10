#include "xtest_input.hpp"

#include <stdexcept>

// Last: Xlib defines None, Button1 and the like as macros.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

namespace opensu::session {

struct XTestInput::Connection {
    Display* display{};

    void key(KeyCode code, bool down) const {
        XTestFakeKeyEvent(display, code, down ? True : False, CurrentTime);
    }
    /// Presses and lets go of `code`, holding Shift around it when `shifted`.
    void tap(KeyCode code, bool shifted) const {
        const KeyCode shift = XKeysymToKeycode(display, XK_Shift_L);
        if (shifted) {
            key(shift, true);
        }
        key(code, true);
        key(code, false);
        if (shifted) {
            key(shift, false);
        }
        XFlush(display);
    }
};

XTestInput::XTestInput(const std::string& name) : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(name.c_str());
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display " + name};
    }
    int event = 0;
    int error = 0;
    int major = 0;
    int minor = 0;
    if (XTestQueryExtension(connection_->display, &event, &error, &major, &minor) == False) {
        XCloseDisplay(connection_->display);
        throw std::runtime_error{"the X display " + name + " has no XTest"};
    }
}

XTestInput::~XTestInput() {
    XCloseDisplay(connection_->display);
}

void XTestInput::key(unsigned keycode, bool down) {
    connection_->key(static_cast<KeyCode>(keycode), down);
}

void XTestInput::button(unsigned button, bool down) {
    XTestFakeButtonEvent(connection_->display, button, down ? True : False, CurrentTime);
}

void XTestInput::move(double x, double y) {
    Display* display = connection_->display;
    const int screen = DefaultScreen(display);
    const int width = DisplayWidth(display, screen);
    const int height = DisplayHeight(display, screen);
    XTestFakeMotionEvent(display, screen, static_cast<int>(x * (width - 1)),
                         static_cast<int>(y * (height - 1)), CurrentTime);
}

bool XTestInput::type(char character) {
    // Printable ASCII's keysyms are its codes (X11/keysymdef.h, Latin 1).
    if (character < ' ' || character > '~') {
        return false;
    }
    const auto symbol = static_cast<KeySym>(static_cast<unsigned char>(character));
    Display* display = connection_->display;
    const KeyCode code = XKeysymToKeycode(display, symbol);
    if (code == 0) {
        return false;
    }
    for (const unsigned level : {0U, 1U}) {
        if (XkbKeycodeToKeysym(display, code, 0, static_cast<int>(level)) == symbol) {
            connection_->tap(code, level == 1);
            return true;
        }
    }
    return false;
}

bool XTestInput::tap(EditKey key) {
    const KeySym symbol = key == EditKey::Backspace ? XK_BackSpace
                          : key == EditKey::Return  ? XK_Return
                          : key == EditKey::Left    ? XK_Left
                                                    : XK_Right;
    const KeyCode code = XKeysymToKeycode(connection_->display, symbol);
    if (code == 0) {
        return false;
    }
    connection_->tap(code, false);
    return true;
}

void XTestInput::flush() {
    XFlush(connection_->display);
}

} // namespace opensu::session
