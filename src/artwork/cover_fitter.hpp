// cover_fitter — covers that do not fit their tile: on a worker, each game's cover is decoded for
// its size, and one of another shape than the tile's (it would be cropped or letterboxed) gets
// SteamGridDB's first cover of the tile's shape, kept as its fitted cover (`CoverOverrides`). A
// player's own game, which no store has a cover for, gets one the same way by its title. Only
// while SteamGridDB has a key; a game with a chosen cover is left alone.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "cover_overrides.hpp"
#include "library/game.hpp"
#include "net/web_client.hpp"
#include "steamgriddb.hpp"

namespace opensu::artwork {

/// A game's fitted cover, now on disk.
struct FittedCover {
    std::string id;
    std::filesystem::path cover;
};

class CoverFitter {
  public:
    CoverFitter(const CoverOverrides& overrides, const SteamGridDb& steamGridDb);
    ~CoverFitter();
    CoverFitter(const CoverFitter&) = delete;
    CoverFitter& operator=(const CoverFitter&) = delete;

    /// Replaces the queue with the games whose cover may not fit a tile of `shape`.
    void request(const std::vector<library::Game>& games, CoverShape shape);
    /// Adds one game, whose cover just arrived.
    void check(const library::Game& game, CoverShape shape);

    /// The covers fitted since the last call.
    [[nodiscard]] std::vector<FittedCover> take();
    /// Whether nothing is queued or being fitted.
    [[nodiscard]] bool idle() const;

  private:
    struct Work {
        library::Game game;
        CoverShape shape{CoverShape::Square};
        /// SteamGridDB did not answer for it once already this round.
        bool retried{false};
    };

    [[nodiscard]] bool wanted(const library::Game& game) const;
    void run(const std::stop_token& stop);
    /// What the worker does after one game.
    enum class Outcome : std::uint8_t {
        Next,
        /// SteamGridDB did not answer: a pause, and the game once more at the end of the round.
        Retry,
        /// No key, a refused key or a cover that cannot be kept: the round ends.
        Stop,
    };

    /// Fits one game.
    Outcome fit(const Work& work);

    const CoverOverrides& overrides_;
    const SteamGridDb& steamGridDb_;
    net::WebClient web_;
    /// The covers already seen to fit, as `<id>\n<file>`. Worker only.
    std::set<std::string> fits_;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Work> queue_;
    std::vector<FittedCover> done_;
    bool busy_{false};
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace opensu::artwork
