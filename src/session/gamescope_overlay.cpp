#include "gamescope_overlay.hpp"

#include <stdexcept>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "lucent/log.h"

namespace opensu::session {

struct GamescopeOverlay::Connection {
    Display* display{};
    Window window{};
    Atom overlay{};
    Atom opacity{};
    Atom inputFocus{};

    /// A CARDINAL property value.
    struct Cardinal {
        unsigned long value;
    };

    void set(Atom property, Cardinal cardinal) const {
        // Format-32 property data is an array of long, whatever its width.
        const long data = static_cast<long>(cardinal.value);
        XChangeProperty(display, window, property, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&data), 1);
    }
    void remove(Atom property) const {
        XDeleteProperty(display, window, property);
    }
    void flush() const {
        XFlush(display);
    }
};

namespace {

// _NET_WM_WINDOW_OPACITY: 0 is transparent, 0xffffffff opaque.
constexpr unsigned long opaque = 0xffffffffUL;

} // namespace

GamescopeOverlay::GamescopeOverlay(std::uint64_t window)
    : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(nullptr);
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display for the Gamescope overlay"};
    }
    connection_->window = static_cast<Window>(window);
    connection_->overlay = XInternAtom(connection_->display, "STEAM_OVERLAY", False);
    connection_->opacity = XInternAtom(connection_->display, "_NET_WM_WINDOW_OPACITY", False);
    connection_->inputFocus = XInternAtom(connection_->display, "STEAM_INPUT_FOCUS", False);
    XWindowAttributes attributes{};
    XGetWindowAttributes(connection_->display, connection_->window, &attributes);
    // Gamescope treats a 24-bit window as opaque (rendervulkan.cpp:2556-2575).
    if (attributes.depth != 32) {
        lucent::error("session", "window depth is {}, not 32: the overlay will hide the game",
                      attributes.depth);
    }
    lucent::info("session", "overlay window 0x{:x}, depth {}, {}x{}", connection_->window,
                 attributes.depth, attributes.width, attributes.height);
}

GamescopeOverlay::~GamescopeOverlay() {
    XCloseDisplay(connection_->display);
}

void GamescopeOverlay::enter() {
    connection_->set(connection_->opacity, {0});
    connection_->set(connection_->overlay, {1});
    connection_->flush();
}

void GamescopeOverlay::setShown(bool shown) {
    connection_->set(connection_->opacity, {shown ? opaque : 0});
    connection_->set(connection_->inputFocus, {shown ? 1UL : 0UL});
    connection_->flush();
}

void GamescopeOverlay::leave() {
    connection_->remove(connection_->overlay);
    connection_->remove(connection_->inputFocus);
    connection_->remove(connection_->opacity);
    connection_->flush();
}

} // namespace opensu::session
