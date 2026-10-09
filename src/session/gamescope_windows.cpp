#include "gamescope_windows.hpp"

#include <algorithm>
#include <climits>
#include <stdexcept>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace opensu::session {

struct GamescopeWindows::Connection {
    Display* display{};
    Atom focusable{};
};

GamescopeWindows::GamescopeWindows() : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(nullptr);
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display to watch Gamescope's windows"};
    }
    connection_->focusable =
        XInternAtom(connection_->display, "GAMESCOPE_FOCUSABLE_WINDOWS", False);
}

GamescopeWindows::~GamescopeWindows() {
    XCloseDisplay(connection_->display);
}

std::vector<pid_t> GamescopeWindows::owners() {
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(
        connection_->display, DefaultRootWindow(connection_->display), connection_->focusable, 0,
        LONG_MAX, False, XA_CARDINAL, &type, &format, &count, &remaining, &data);
    std::vector<pid_t> pids;
    if (status == Success && data != nullptr && format == 32) {
        // Format-32 property data is an array of long, whatever its width.
        const auto* values = reinterpret_cast<const long*>(data);
        for (unsigned long i = 2; i < count; i += 3) {
            pids.push_back(static_cast<pid_t>(values[i]));
        }
    }
    if (data != nullptr) {
        XFree(data);
    }
    return pids;
}

bool GamescopeWindows::anyOwnedBy(const std::vector<pid_t>& pids) {
    return std::ranges::any_of(owners(), [&pids](pid_t owner) {
        return std::ranges::find(pids, owner) != pids.end();
    });
}

} // namespace opensu::session
