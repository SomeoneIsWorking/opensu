// player_profiles — the shell's profiles: the book in this run's files, opened before anything
// reads saves, the saves from before profiles moved into the first profile's as it opens
// (`profiles::moveIntoProfile`), and what the save owners are told of the profile a game plays as:
// its id and the runner's folder for it.
#pragma once

#include <filesystem>
#include <string>

#include "player_files.hpp"
#include "profiles/profile_book.hpp"

namespace opensu::app {

class PlayerProfiles {
  public:
    /// Opens the book in `files` and moves the saves under `runnerSaves` (`Config::savesDir`) and
    /// `files`' state dir that predate profiles; throws `std::runtime_error` when the book cannot
    /// be kept.
    PlayerProfiles(const PlayerFiles& files, std::filesystem::path runnerSaves);

    [[nodiscard]] profiles::ProfileBook& book() noexcept {
        return book_;
    }
    [[nodiscard]] const profiles::ProfileBook& book() const noexcept {
        return book_;
    }
    /// Why the saves from before profiles could not all be moved; empty when they were.
    [[nodiscard]] const std::string& moveProblem() const noexcept {
        return moveProblem_;
    }
    /// The runner's saves root, which each profile's folder is under.
    [[nodiscard]] const std::filesystem::path& runnerSaves() const noexcept {
        return runnerSaves_;
    }

  private:
    profiles::ProfileBook book_;
    std::filesystem::path runnerSaves_;
    std::string moveProblem_;
};

} // namespace opensu::app
