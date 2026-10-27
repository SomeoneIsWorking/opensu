#include "root_properties.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace opensu::session {

struct RootProperties::Connection {
    Display* display{};

    [[nodiscard]] Atom atom(std::string_view name) const {
        return XInternAtom(display, std::string{name}.c_str(), False);
    }
};

RootProperties::RootProperties(std::string_view purpose)
    : connection_{std::make_unique<Connection>()} {
    connection_->display = XOpenDisplay(nullptr);
    if (connection_->display == nullptr) {
        throw std::runtime_error{"cannot open the X display to " + std::string{purpose}};
    }
}

RootProperties::~RootProperties() {
    XCloseDisplay(connection_->display);
}

void RootProperties::setCardinal(std::string_view name, long value) {
    // Format-32 property data is an array of long, whatever its width.
    XChangeProperty(connection_->display, DefaultRootWindow(connection_->display),
                    connection_->atom(name), XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&value), 1);
}

void RootProperties::setCardinals(std::string_view name, std::span<const std::uint32_t> values) {
    const std::vector<long> data(values.begin(), values.end());
    XChangeProperty(connection_->display, DefaultRootWindow(connection_->display),
                    connection_->atom(name), XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(data.data()),
                    static_cast<int>(data.size()));
}

void RootProperties::setText(std::string_view name, std::string_view value) {
    XChangeProperty(connection_->display, DefaultRootWindow(connection_->display),
                    connection_->atom(name), connection_->atom("UTF8_STRING"), 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(value.data()),
                    static_cast<int>(value.size()));
}

void RootProperties::flush() {
    XFlush(connection_->display);
}

} // namespace opensu::session
