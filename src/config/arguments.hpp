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
    /// `--steam`: a hidden run starts the Steam client in its mode as an ordinary run does; the
    /// maintainer ends the run with POST /quit so Steam is shut down with it.
    bool steam{false};
    /// `--sound`: a hidden run plays the UI sounds on the default audio device as an ordinary run
    /// does; SDL's and the sound server's environment can route them elsewhere.
    bool sound{false};
    bool help{false};

    /// Whether the shell starts the Steam client: a hidden run only with `--steam`, so a killed
    /// one leaves no Steam behind.
    [[nodiscard]] bool startsSteam() const noexcept {
        return !hidden || steam;
    }

    /// Whether the shell opens the audio device: a hidden run only with `--sound`.
    [[nodiscard]] bool playsSounds() const noexcept {
        return !hidden || sound;
    }

    [[nodiscard]] static Arguments parse(std::span<const std::string> args);
};

} // namespace opensu::config
