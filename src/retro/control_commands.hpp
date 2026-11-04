// control_commands — what each control channel command does to the running game, run by the frame
// loop between frames: the state slot saved and loaded, reset, live video settings, the core's
// options listed and changed, and the last frame read back in hidden runs.
#pragma once

#include <string>

#include "control_server.hpp"
#include "core.hpp"
#include "core_options.hpp"
#include "presenter.hpp"
#include "save_state.hpp"

namespace opensu::retro {

/// The last image `presenter` showed as PNG; empty with `error` when there is none or it fails.
[[nodiscard]] std::string framePng(Presenter& presenter, std::string& error);

class ControlCommands {
  public:
    struct Parts {
        Core& core;
        SaveState& state;
        Presenter& presenter;
        CoreOptions& options;
        /// Whether the run is hidden, the only one whose frame is read back.
        bool hidden;
    };

    explicit ControlCommands(Parts parts) : parts_{parts} {
    }

    /// Runs `request` on the game. Frame loop only, between frames.
    [[nodiscard]] ControlAnswer perform(const ControlRequest& request);

  private:
    Parts parts_;
};

} // namespace opensu::retro
