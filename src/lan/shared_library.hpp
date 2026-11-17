// shared_library — the host's library as paired openSUs are given it: the games installed here
// (not those of other openSUs), each by its id and title, what it runs as and the ways it can be
// launched, with a token for its cover that changes when the cover does, and why this openSU
// cannot stream them now. A cover is sent only by its token, so a peer can never name a file. Safe
// from any thread.
#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game.hpp"
#include "wire.hpp"

namespace opensu::lan {

class SharedLibrary {
  public:
    /// Shares the installed games among `games` from now on; `hosting` is why they cannot be
    /// streamed from here now, empty when they can.
    void share(const std::vector<library::Game>& games, std::string hosting);

    /// The game `id` runs here with these profiles in its seats; an empty id, none runs.
    void playing(std::string id, std::vector<std::string> profiles);

    [[nodiscard]] wire::Listing listing() const;
    /// The cover file `token` stands for; nothing for a token not shared now.
    [[nodiscard]] std::optional<std::filesystem::path> cover(std::string_view token) const;

  private:
    std::string playingId_;
    std::vector<std::string> playingAs_;
    mutable std::mutex mutex_;
    std::vector<wire::ListedGame> games_;
    std::string hosting_;
    std::map<std::string, std::filesystem::path, std::less<>> covers_;
};

} // namespace opensu::lan
