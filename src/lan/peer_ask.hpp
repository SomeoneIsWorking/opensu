// peer_ask — one paired openSU's ask of this one that the player answers: one ask at a time, by
// peer and what it is about, put to the player as a question, the verdict the asker's polls get
// (collected once it is given), lapsing after `patience` like a pairing. The owners of each kind of
// ask (`ProfileCopies`, `LoginCopies`) keep one and say what an ask carries (`Subject`). Safe from
// any thread: the LAN listener's and the loop.
#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "host_pairing.hpp"
#include "peer_book.hpp"

namespace opensu::lan {

template <typename Subject> class PeerAsk {
  public:
    using Clock = std::chrono::steady_clock;

    /// `peer` asks about `about`, carrying `subject`. False with `refusal` while another ask waits
    /// for the player; the same ask again keeps its place.
    [[nodiscard]] bool ask(const Peer& peer, std::string about, Subject subject,
                           Clock::time_point now, std::string& refusal) {
        const std::lock_guard lock{mutex_};
        lapse(now);
        if (pending_ && pending_->peer.id == peer.id && pending_->about == about) {
            return true;
        }
        if (pending_ && pending_->verdict == Verdict::Waiting) {
            refusal = "another openSU's ask waits for the player";
            return false;
        }
        pending_ = Pending{.peer = peer,
                           .about = std::move(about),
                           .subject = std::move(subject),
                           .started = now,
                           .verdict = Verdict::Waiting};
        return true;
    }

    struct Status {
        Verdict verdict{Verdict::Waiting};
        Subject subject;
    };
    /// How `peerId`'s ask about `about` stands; nothing when no such ask is under way. A verdict
    /// given is collected: the ask is then over.
    [[nodiscard]] std::optional<Status> status(std::string_view peerId, std::string_view about,
                                               Clock::time_point now) {
        const std::lock_guard lock{mutex_};
        lapse(now);
        const Pending* const asked = find(peerId, about);
        if (asked == nullptr) {
            return std::nullopt;
        }
        Status status{.verdict = asked->verdict, .subject = asked->subject};
        if (asked->verdict != Verdict::Waiting) {
            pending_.reset();
        }
        return status;
    }

    /// What the player is asked.
    struct Question {
        Peer peer;
        std::string about;
        Subject subject;

        bool operator==(const Question&) const = default;
    };
    [[nodiscard]] std::optional<Question> question(Clock::time_point now) const {
        const std::lock_guard lock{mutex_};
        if (!pending_ || pending_->verdict != Verdict::Waiting ||
            now - pending_->started > patience) {
            return std::nullopt;
        }
        return Question{
            .peer = pending_->peer, .about = pending_->about, .subject = pending_->subject};
    }

    /// The player's answer to `peerId`'s ask about `about`: `verdictOf(peer, subject)`, called
    /// under the lock, gives the verdict its next poll is told. False when no such ask is under
    /// way.
    template <typename VerdictOf>
    [[nodiscard]] bool decide(std::string_view peerId, std::string_view about,
                              Clock::time_point now, VerdictOf&& verdictOf) {
        const std::lock_guard lock{mutex_};
        lapse(now);
        Pending* const asked = find(peerId, about);
        if (asked == nullptr) {
            return false;
        }
        asked->verdict = std::forward<VerdictOf>(verdictOf)(std::as_const(asked->peer),
                                                            std::as_const(asked->subject));
        return true;
    }

  private:
    struct Pending {
        Peer peer;
        std::string about;
        Subject subject;
        Clock::time_point started;
        Verdict verdict{Verdict::Waiting};
    };

    /// Drops an ask past its time. Under `mutex_`.
    void lapse(Clock::time_point now) {
        if (pending_ && now - pending_->started > patience) {
            pending_.reset();
        }
    }
    /// The ask under way when it is `peerId`'s about `about`, else null. Under `mutex_`.
    [[nodiscard]] Pending* find(std::string_view peerId, std::string_view about) {
        if (!pending_.has_value() || pending_->peer.id != peerId || pending_->about != about) {
            return nullptr;
        }
        return &*pending_;
    }

    mutable std::mutex mutex_;
    std::optional<Pending> pending_;
};

} // namespace opensu::lan
