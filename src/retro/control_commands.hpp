// control_commands — what each control channel command does to the running game, run by the frame
// loop between frames: the state slot saved and loaded, reset, live video settings, Rotate screen,
// the core's
// options listed and changed, the last frame read back, and Save and close, after
// which the game runs no more frames.
#pragma once

#include <string>

#include "control_server.hpp"
#include "core.hpp"
#include "core_options.hpp"
#include "presenter.hpp"
#include "resume_state.hpp"
#include "save_memory.hpp"
#include "save_state.hpp"
#include "screen_turn.hpp"

namespace opensu::retro {

/// The last image `presenter` showed as PNG; empty with `error` when there is none or it fails.
[[nodiscard]] std::string framePng(Presenter& presenter, std::string& error);

class ControlCommands {
  public:
    struct Parts {
        Core& core;
        SaveState& state;
        SaveMemory& saves;
        ResumeState& resume;
        Presenter& presenter;
        CoreOptions& options;
        ScreenTurn& screen;
        /// Whether the run is a netplay session, whose machines must stay the same.
        bool netplay;
    };

    explicit ControlCommands(Parts parts) : parts_{parts} {
    }

    /// Runs `request` on the game. Frame loop only, between frames.
    [[nodiscard]] ControlAnswer perform(const ControlRequest& request);

    /// Whether Save and close was done: the game waits to be closed, its frames stopped.
    [[nodiscard]] bool suspended() const noexcept {
        return suspended_;
    }

    /// Whether the shell's Guide asks the game to wait for it.
    [[nodiscard]] bool held() const noexcept {
        return held_;
    }

  private:
    [[nodiscard]] ControlAnswer suspend();

    Parts parts_;
    bool suspended_{false};
    bool held_{false};
};

} // namespace opensu::retro
