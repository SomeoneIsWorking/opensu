// shared_library — the host's library as paired openSUs are given it: the games installed here
// (not those of other openSUs), each by its id and title, with a token for its cover that changes
// when the cover does. A cover is sent only by its token, so a peer can never name a file. Safe
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
    /// Shares the installed games among `games` from now on.
    void share(const std::vector<library::Game>& games);

    [[nodiscard]] std::vector<wire::ListedGame> listing() const;
    /// The cover file `token` stands for; nothing for a token not shared now.
    [[nodiscard]] std::optional<std::filesystem::path> cover(std::string_view token) const;

  private:
    mutable std::mutex mutex_;
    std::vector<wire::ListedGame> games_;
    std::map<std::string, std::filesystem::path, std::less<>> covers_;
};

} // namespace opensu::lan
