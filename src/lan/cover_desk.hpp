// cover_desk — where a paired guest's asks about a game's cover meet this openSU's main loop: the
// listener hands over which game's covers are wanted, a cover picked and a match picked, and reads
// back what the loop last published. A thumbnail is sent only by its token, so a peer can never
// name a file. One listing at a time: the last game asked for. Safe from any thread.
#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "wire.hpp"

namespace opensu::lan {

class CoverDesk {
  public:
    /// Thumbnails by their tokens.
    using Thumbs = std::map<std::string, std::filesystem::path, std::less<>>;

    /// The token `file` is offered under.
    [[nodiscard]] static std::string tokenOf(const std::filesystem::path& file);

    // The listener's side.
    /// What is offered for `ask.game` so far; a fresh ask, or another game, starts its listing.
    [[nodiscard]] wire::CoverOffersAnswer offers(const wire::CoverAsk& ask);
    void pick(wire::CoverPick pick);
    void match(wire::GameMatch match);
    /// The thumbnail `token` stands for; nothing for one not offered now.
    [[nodiscard]] std::optional<std::filesystem::path> thumb(std::string_view token) const;

    // The main loop's side.
    /// The game whose listing was asked for since the last call.
    [[nodiscard]] std::optional<std::string> takeWanted();
    [[nodiscard]] std::vector<wire::CoverPick> takePicks();
    [[nodiscard]] std::vector<wire::GameMatch> takeMatches();
    /// `answer` is what is offered now, its thumbnails `thumbs`; the pick's outcome stands.
    void publish(wire::CoverOffersAnswer answer, Thumbs thumbs);
    /// The pick for `game` ended: `wire::coverKept`, or why not.
    void picked(const std::string& game, std::string outcome);

  private:
    mutable std::mutex mutex_;
    std::optional<std::string> wanted_;
    wire::CoverOffersAnswer answer_;
    Thumbs thumbs_;
    std::vector<wire::CoverPick> picks_;
    std::vector<wire::GameMatch> matches_;
};

} // namespace opensu::lan
