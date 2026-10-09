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
    /// `--session`: openSU is the login session, started by a session entry or the display
    /// manager, rather than an app on a desktop: Gamescope runs top level. The power list then
    /// offers Switch to desktop.
    bool loginSession{false};
    bool help{false};

    /// Whether the shell starts the Steam client: a hidden run never does, so it leaves no Steam
    /// behind when it is killed.
    [[nodiscard]] bool startsSteam() const noexcept {
        return !hidden;
    }

    [[nodiscard]] static Arguments parse(std::span<const std::string> args);
};

} // namespace opensu::config
