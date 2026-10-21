#include "steam_window_hiding.hpp"

#include <stdexcept>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "lucent/log.h"

namespace opensu::session {

struct SteamWindowHiding::Connection {
    Display* display{};
    Atom hide{};
};

SteamWindowHiding::SteamWindowHiding() : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(nullptr);
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display to hide Steam's windows"};
    }
    connection_->hide = XInternAtom(connection_->display, "GAMESCOPE_HIDE_STEAM_CLIENT", False);
}

SteamWindowHiding::~SteamWindowHiding() {
    XCloseDisplay(connection_->display);
}

void SteamWindowHiding::hide(bool hides) {
    // Format-32 property data is an array of long, whatever its width.
    const long value = hides ? 1 : 0;
    XChangeProperty(connection_->display, DefaultRootWindow(connection_->display),
                    connection_->hide, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&value), 1);
    XFlush(connection_->display);
    lucent::info("session", "GAMESCOPE_HIDE_STEAM_CLIENT {}: Steam's windows {} in Gamescope",
                 value, hides ? "hidden" : "shown");
}

} // namespace opensu::session
