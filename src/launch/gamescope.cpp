#include "gamescope.hpp"

#include <string>

namespace iideck::launch {

library::LaunchSpec wrapInGamescope(const Output& output, const library::LaunchSpec& inner) {
    library::LaunchSpec wrapped;
    wrapped.program = "gamescope";
    const std::string width = std::to_string(output.width);
    const std::string height = std::to_string(output.height);
    wrapped.args = {"-W", width, "-H", height, "-w", width, "-h", height};
    if (output.refreshHz > 0) {
        wrapped.args.push_back("-r");
        wrapped.args.push_back(std::to_string(output.refreshHz));
    }
    wrapped.args.push_back("-f");
    wrapped.args.push_back("--");
    wrapped.args.push_back(inner.program);
    wrapped.args.insert(wrapped.args.end(), inner.args.begin(), inner.args.end());
    return wrapped;
}

} // namespace iideck::launch
