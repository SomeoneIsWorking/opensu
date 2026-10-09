// arguments — the command line, parsed once into typed values.
#pragma once

#include <optional>
#include <span>
#include <string>

namespace opensu::config {

struct Arguments {
    /// `--render FILE`: render one frame to FILE and exit.
    std::optional<std::string> renderPath;
    /// `--keyboard`: with `--render`, draw the keyboard's prompts.
    bool keyboardPrompts{false};
    /// `--hidden`: the interactive shell with an unmapped window, no Gamescope session, no pads
    /// and a free control port, for maintainer runs and tests.
    bool hidden{false};
    bool help{false};

    [[nodiscard]] static Arguments parse(std::span<const std::string> args);
};

} // namespace opensu::config
