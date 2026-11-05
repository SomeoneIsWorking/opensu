#include "player_profiles.hpp"

#include <chrono>
#include <utility>

#include "lucent/log.h"
#include "profiles/save_move.hpp"

namespace opensu::app {

PlayerProfiles::PlayerProfiles(const PlayerFiles& files, std::filesystem::path runnerSaves)
    : book_{files.profilesFile()}, runnerSaves_{std::move(runnerSaves)} {
    try {
        static_cast<void>(
            profiles::moveIntoProfile(profiles::SaveMove{.saves = runnerSaves_,
                                                         .state = files.stateDir(),
                                                         .versions = files.saveVersionsFile()},
                                      book_.first().id, std::chrono::system_clock::now()));
    } catch (const std::filesystem::filesystem_error& failure) {
        moveProblem_ = failure.what();
        lucent::error("profiles",
                      "the saves from before profiles cannot all be moved into {}'s: {}",
                      book_.first().name, moveProblem_);
    }
}

} // namespace opensu::app
