// close_asks — a paired openSU's asks to close the game running here (`/lan/close`): one whose
// asking profile is linked with a profile playing it closes it with no question, any other waits
// for the player here (`question`, `answer`, Keep playing focused). The game is closed on the loop
// through `close`, saved first when it runs on a built-in core; the asker hears "closed" once
// nothing runs here.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "lan/peer_book.hpp"
#include "lan/wire.hpp"

namespace opensu::app {

class CloseAsks {
  public:
    /// Closes the game running here, saving it first where it can; why it did not, or empty.
    using Close = std::function<std::string()>;

    explicit CloseAsks(Close close) : close_{std::move(close)} {
    }

    /// The game running here now: its id (empty when none), title and the profiles in its seats.
    /// Main loop.
    void playing(std::string id, std::string title, std::vector<std::string> seats);

    /// `peer` asks, as `profile` (`linked` with it), to close `gameId`; how that stands. Listener
    /// thread.
    [[nodiscard]] lan::wire::PlayVerdict ask(const lan::Peer& peer, const std::string& gameId,
                                             const std::string& profile, bool linked);

    /// A peer's ask the player here is to answer.
    struct Question {
        lan::Peer peer;
        std::string title;

        bool operator==(const Question&) const = default;
    };
    [[nodiscard]] std::optional<Question> question() const;
    /// The player's answer to `peerId`'s ask. Main loop.
    void answer(const std::string& peerId, bool allow);

    /// Closes the game when an ask was let through, and lapses unanswered asks. Main loop.
    void service();

  private:
    using Clock = std::chrono::steady_clock;
    struct Asked {
        enum class Verdict : std::uint8_t { Waiting, Allowed, Declined };
        lan::Peer peer;
        std::string gameId;
        Verdict verdict{Verdict::Waiting};
        std::string why;
        Clock::time_point at;
    };
    /// How long the player here may take to answer, and an answer is kept for its asker.
    static constexpr std::chrono::seconds patience{120};
    static constexpr std::chrono::seconds answerKept{30};

    Close close_;
    mutable std::mutex mutex_;
    std::string runningId_;
    std::string runningTitle_;
    std::vector<std::string> seats_;
    std::vector<Asked> asks_;
    /// A close let through and not yet done.
    bool closing_{false};
};

} // namespace opensu::app
