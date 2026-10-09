#include "arguments.hpp"

namespace opensu::config {

Arguments Arguments::parse(std::span<const std::string> args) {
    Arguments parsed;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--render" && i + 1 < args.size()) {
            parsed.renderPath = args[++i];
        } else if (args[i] == "--keyboard") {
            parsed.keyboardPrompts = true;
        } else if (args[i] == "--hidden") {
            parsed.hidden = true;
        } else if (args[i] == "--session") {
            parsed.loginSession = true;
        } else if (args[i] == "--help" || args[i] == "-h") {
            parsed.help = true;
        }
    }
    return parsed;
}

} // namespace opensu::config
