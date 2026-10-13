// save_state — the game's one state slot, `<saves>/<rom stem>.state`: the core's serialized state
// written whole, and put back into the core.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core.hpp"

namespace opensu::retro {

enum class StateResult : std::uint8_t {
    Done,
    /// The core cannot serialize, or refused the state it was given.
    Refused,
    /// No state is saved for this game.
    Missing,
    /// The slot could not be written or read.
    FileFailed,
};

struct StateOutcome {
    StateResult result{StateResult::Done};
    /// Why it did not happen; empty when it did.
    std::string reason;
};

class SaveState {
  public:
    explicit SaveState(std::filesystem::path file);

    /// Serializes the core into the slot, replacing what was there only once the write is whole.
    StateOutcome save(Core& core);
    /// Loads the slot into the core.
    StateOutcome load(Core& core);

    /// Whether the slot holds a state.
    [[nodiscard]] bool exists() const noexcept {
        return exists_;
    }
    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
    bool exists_{false};
};

} // namespace opensu::retro
