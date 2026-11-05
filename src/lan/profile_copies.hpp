// profile_copies — the owner's half of a paired openSU copying one of this openSU's profiles: one
// ask at a time, put to the player as a question (Decline or Allow), the verdict the asker's polls
// get, lapsing after `patience` like a pairing. Allowed, the profile is linked with the asker in
// the book before the verdict says so. Safe from any thread: the LAN listener's and the loop.
#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "host_pairing.hpp"
#include "peer_book.hpp"
#include "profiles/profile_book.hpp"

namespace opensu::lan {

class ProfileCopies {
  public:
    using Clock = std::chrono::steady_clock;

    explicit ProfileCopies(profiles::ProfileBook& profiles) : profiles_{profiles} {
    }

    /// `peer` asks for a copy of `profileId`. False with `refusal` when there is no such profile or
    /// another ask waits for the player; the same ask again keeps its place.
    [[nodiscard]] bool ask(const Peer& peer, std::string_view profileId, Clock::time_point now,
                           std::string& refusal);
    /// The verdict on `peer`'s ask for `profileId`, with the profile once allowed; nothing when no
    /// such ask is under way.
    struct Status {
        Verdict verdict{Verdict::Waiting};
        profiles::Profile profile;
    };
    [[nodiscard]] std::optional<Status> status(std::string_view peerId, std::string_view profileId,
                                               Clock::time_point now);

    /// What the player is asked.
    struct Question {
        Peer peer;
        profiles::Profile profile;

        bool operator==(const Question&) const = default;
    };
    [[nodiscard]] std::optional<Question> question(Clock::time_point now) const;
    /// The player's answer to `peerId`'s ask for `profileId`. False with `error` when an allowed
    /// copy's link could not be kept; the asker is then told it was declined.
    [[nodiscard]] bool decide(std::string_view peerId, std::string_view profileId, bool allow,
                              Clock::time_point now, std::string& error);

  private:
    struct Pending {
        Peer peer;
        profiles::Profile profile;
        Clock::time_point started;
        Verdict verdict{Verdict::Waiting};
    };
    /// Drops an ask past its time. Under `mutex_`.
    void lapse(Clock::time_point now);

    profiles::ProfileBook& profiles_;
    mutable std::mutex mutex_;
    std::optional<Pending> pending_;
};

} // namespace opensu::lan
