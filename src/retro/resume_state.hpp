// resume_state — Save and close and the resume after it: the game's state taken with the core and
// the save RAM it goes with (`resume_file`), and put back as the runner starts the game, before its
// first frame, when the same core and the same save RAM are there. A state another core wrote is
// left for that core; one this core cannot use is removed, as is one once it is loaded.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "core.hpp"
#include "save_state.hpp"

namespace opensu::retro {

/// How the start went with the game's resume state.
struct ResumeReport {
    /// When the state the game resumed from was taken; nothing when it did not resume.
    std::optional<std::chrono::system_clock::time_point> from;
    /// Why a resume state was there and not loaded; empty when none was refused.
    std::string refused;
};

class ResumeState {
  public:
    /// `core` is the running core's name as `coreName` gives it.
    ResumeState(std::filesystem::path file, std::string core);

    /// Puts the game back where Save and close left it. Right after the game and its save RAM are
    /// loaded, before the first frame; a refused state leaves the game as it booted.
    [[nodiscard]] ResumeReport resume(Core& core);
    /// Takes the state Save and close leaves, with the save RAM as it is now.
    [[nodiscard]] StateOutcome suspend(Core& core);

  private:
    std::filesystem::path file_;
    std::string core_;
};

} // namespace opensu::retro
