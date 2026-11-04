#include "gamescope.hpp"

#include <string_view>

namespace opensu::session {
namespace {

/// Gamescope's children get its own socket as WAYLAND_DISPLAY, so a Wayland game shows inside it.
constexpr std::string_view exposeWayland = "--expose-wayland";

} // namespace

std::vector<std::string> gamescopeArgs(const display::Output& output, const std::string& program,
                                       const std::vector<std::string>& args) {
    if (output.native()) {
        // Top level on the seat: the DRM backend takes the connector's preferred mode.
        std::vector<std::string> top{"--backend", "drm", std::string{exposeWayland}, "--", program};
        top.insert(top.end(), args.begin(), args.end());
        return top;
    }
    const std::string width = std::to_string(output.width);
    const std::string height = std::to_string(output.height);
    std::vector<std::string> out{"-W", width, "-H", height, "-w", width, "-h", height};
    if (output.refreshHz > 0) {
        out.push_back("-r");
        out.push_back(std::to_string(output.refreshHz));
    }
    out.push_back("-f");
    // The fork: a host close request closes the focused game, not the session.
    out.push_back("--close-focused-window");
    out.push_back(std::string{exposeWayland});
    out.push_back("--");
    out.push_back(program);
    out.insert(out.end(), args.begin(), args.end());
    return out;
}

} // namespace opensu::session
