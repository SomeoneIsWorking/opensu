#include "gamescope_windows.hpp"

#include <algorithm>
#include <climits>
#include <span>
#include <stdexcept>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace opensu::session {

struct GamescopeWindows::Connection {
    Display* display{};
    Atom focusable{};
    Atom focused{};

    /// The root's CARDINAL property, as longs: format-32 data is an array of long, whatever its
    /// width.
    [[nodiscard]] std::vector<long> cardinals(Atom property) const {
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* data = nullptr;
        const int status =
            XGetWindowProperty(display, DefaultRootWindow(display), property, 0, LONG_MAX, False,
                               XA_CARDINAL, &type, &format, &count, &remaining, &data);
        std::vector<long> values;
        if (status == Success && data != nullptr && format == 32) {
            const std::span<const long> read{reinterpret_cast<const long*>(data), count};
            values.assign(read.begin(), read.end());
        }
        if (data != nullptr) {
            XFree(data);
        }
        return values;
    }
};

GamescopeWindows::GamescopeWindows() : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(nullptr);
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display to watch Gamescope's windows"};
    }
    connection_->focusable =
        XInternAtom(connection_->display, "GAMESCOPE_FOCUSABLE_WINDOWS", False);
    connection_->focused = XInternAtom(connection_->display, "GAMESCOPE_FOCUSED_WINDOW", False);
}

GamescopeWindows::~GamescopeWindows() {
    XCloseDisplay(connection_->display);
}

std::vector<FocusableWindow> GamescopeWindows::focusable() {
    return parseFocusable(connection_->cardinals(connection_->focusable));
}

std::vector<pid_t> GamescopeWindows::owners() {
    std::vector<pid_t> pids;
    for (const FocusableWindow& window : focusable()) {
        pids.push_back(window.pid);
    }
    return pids;
}

bool GamescopeWindows::anyOwnedBy(const std::vector<pid_t>& pids) {
    return std::ranges::any_of(owners(), [&pids](pid_t owner) {
        return std::ranges::find(pids, owner) != pids.end();
    });
}

FocusedWindow GamescopeWindows::focused() {
    const std::vector<long> window = connection_->cardinals(connection_->focused);
    return describeFocus(window.empty() ? 0 : static_cast<std::uint64_t>(window.front()),
                         focusable());
}

std::optional<FocusedWindow> GamescopeWindows::takeChange() {
    Display* display = connection_->display;
    if (!watching_) {
        XSelectInput(display, DefaultRootWindow(display), PropertyChangeMask);
        watching_ = true;
        return focused();
    }
    bool changed = false;
    while (XPending(display) > 0) {
        XEvent event{};
        XNextEvent(display, &event);
        if (event.type == PropertyNotify && (event.xproperty.atom == connection_->focused ||
                                             event.xproperty.atom == connection_->focusable)) {
            changed = true;
        }
    }
    if (!changed) {
        return std::nullopt;
    }
    return focused();
}

} // namespace opensu::session
