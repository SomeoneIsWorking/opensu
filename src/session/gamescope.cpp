#include "gamescope.hpp"

namespace opensu::session {

std::vector<std::string> gamescopeArgs(const Output& output, const std::string& program,
                                       const std::vector<std::string>& args) {
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
    out.push_back("--");
    out.push_back(program);
    out.insert(out.end(), args.begin(), args.end());
    return out;
}

} // namespace opensu::session
