// netplay_asks — this openSU's side of a paired openSU's ask to play a ROM together: one ask at a
// time, put to the player as a question, the verdict the asker's polls get, lapsing after
// `patience` like a pairing; and the save each side hands the other before both start. Safe from
// any thread: the LAN listener's and the loop.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "host_pairing.hpp"
#include "peer_book.hpp"
#include "secrets/crypto.hpp"
#include "wire.hpp"

namespace opensu::lan {

/// The first thing `there`, an asker's, differs in from `here`, in words for the asker: "its core
/// option mgba_frameskip is 1, yours 2". Empty when they agree.
[[nodiscard]] std::string netplayDifference(const wire::NetplayFacts& here,
                                            const wire::NetplayFacts& there);

class NetplayAsks {
  public:
    using Clock = std::chrono::steady_clock;

    /// `peer` asks to play `rom` with `salt` as its half of the session's. False with `refusal`
    /// when another's ask waits for the player; the same ask again keeps its place.
    [[nodiscard]] bool ask(const Peer& peer, const wire::NetplayAsk& rom, Clock::time_point now,
                           std::string& refusal);

    /// What the asker's poll on `sha256` gets; nothing when no such ask is under way.
    [[nodiscard]] std::optional<wire::NetplayVerdict>
    status(std::string_view peerId, std::string_view sha256, Clock::time_point now);

    /// What the player is asked.
    struct Question {
        Peer peer;
        wire::NetplayAsk rom;

        bool operator==(const Question&) const = default;
    };
    [[nodiscard]] std::optional<Question> question(Clock::time_point now) const;

    /// The player said yes to `peerId`'s ask: it is asked no more, and the asker keeps waiting
    /// until `allow` or `decline`, with a new `patience`.
    void accept(std::string_view peerId, Clock::time_point now);
    /// Allows the waiting ask of `peerId`: the UDP port the session's runner here bound and this
    /// side's salt.
    void allow(std::string_view peerId, std::uint16_t port, const secrets::crypto::Bytes& salt,
               Clock::time_point now);
    /// Declines it, the asker told `refusal`.
    void decline(std::string_view peerId, std::string refusal, Clock::time_point now);

    /// `peerId` may now take `save` (empty when this side has none) for the session on `sha256`.
    void handOver(std::string_view peerId, std::string_view sha256, std::string save);
    /// The save handed to `peerId`; nothing when none is. Taken once.
    [[nodiscard]] std::optional<std::string> takeSave(std::string_view peerId,
                                                      std::string_view sha256);

  private:
    struct Pending {
        Peer peer;
        wire::NetplayAsk rom;
        Clock::time_point started;
        wire::NetplayVerdict verdict;
        bool accepted{false};
    };
    struct Handed {
        std::string peerId;
        std::string sha256;
        std::string save;
    };
    /// Drops an ask past its time. Under `mutex_`.
    void lapse(Clock::time_point now);

    mutable std::mutex mutex_;
    std::optional<Pending> pending_;
    std::optional<Handed> handed_;
};

} // namespace opensu::lan
