// host_pairing — the host's half of pairing, one guest at a time. A guest's commitment is answered
// with a fresh X25519 key and nonce, at most one offer per `offerSpacing` until the player answers;
// its reveal is checked against the commitment, and the code and secret follow (`pairing_keys`);
// the player is asked with the code on screen, and the guest's polls get the verdict signed with
// the secret. Accepted, the guest and its secret are handed to the book. A pairing the player has
// not answered in `patience` lapses. Safe from any thread: the LAN listener's threads and the
// shell's loop.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "pairing_keys.hpp"
#include "peer_book.hpp"
#include "secrets/crypto.hpp"

namespace opensu::lan {

/// How long a pairing waits for the player, and the guest for the host.
inline constexpr std::chrono::seconds patience{120};
/// How soon after one offer the next is made, unless the player answered the first. A man in the
/// middle learns the host's code from each offer, so this is what bounds his guesses at the
/// guest's: a dozen in the guest's `patience`, each one in a million.
inline constexpr std::chrono::seconds offerSpacing{10};

/// The host's word on a pairing, as the wire spells it.
enum class Verdict : std::uint8_t { Waiting, Accepted, Declined };

[[nodiscard]] std::string_view key(Verdict verdict) noexcept;

class HostPairing {
  public:
    using Clock = std::chrono::steady_clock;

    explicit HostPairing(std::string hostId) : hostId_{std::move(hostId)} {
    }

    /// What the guest is sent for its commitment.
    struct Offer {
        secrets::crypto::Bytes key;
        secrets::crypto::Bytes nonce;
    };
    /// `guest` asks to pair, committed to `commitment`. Nothing with `refusal` while another guest
    /// is pairing, or within `offerSpacing` of the last offer the player did not answer; the same
    /// guest asking again after that starts over.
    [[nodiscard]] std::optional<Offer> begin(const Peer& guest, secrets::crypto::Bytes commitment,
                                             Clock::time_point now, std::string& refusal);

    /// The guest shows its key and nonce. False with `refusal` when they are not what it committed
    /// to or no pairing of its is under way.
    [[nodiscard]] bool reveal(std::string_view guestId, const secrets::crypto::Bytes& key,
                              const secrets::crypto::Bytes& nonce, Clock::time_point now,
                              std::string& refusal);

    /// The verdict a guest polls for, and its signature.
    struct Status {
        Verdict verdict{Verdict::Waiting};
        std::string signature;
    };
    /// Nothing when `guestId` has no revealed pairing under way.
    [[nodiscard]] std::optional<Status> status(std::string_view guestId, Clock::time_point now);

    /// What the player is asked: whom to pair with, and the code both screens show.
    struct Question {
        Peer guest;
        std::string code;

        bool operator==(const Question&) const = default;
    };
    /// The pairing waiting for the player, if one is.
    [[nodiscard]] std::optional<Question> question(Clock::time_point now) const;

    /// What an accepted pairing leaves the book.
    struct Accepted {
        Peer guest;
        secrets::crypto::Bytes secret;
    };
    /// What the book keeps if the player accepts the question about `guestId`; nothing when that
    /// question is no longer asked.
    [[nodiscard]] std::optional<Accepted> accepting(std::string_view guestId,
                                                    Clock::time_point now) const;
    /// The player's answer to the question about `guestId`, which its next poll is told; Accept
    /// only once the book kept what `accepting` gave.
    void decide(std::string_view guestId, Verdict verdict, Clock::time_point now);

  private:
    struct Pending {
        Peer guest;
        secrets::crypto::Bytes commitment;
        secrets::crypto::KeyPair key;
        secrets::crypto::Bytes nonce;
        Clock::time_point started;
        secrets::crypto::Bytes guestNonce{};
        secrets::crypto::Bytes secret{};
        std::string code{};
        bool revealed{false};
        Verdict verdict{Verdict::Waiting};
    };

    /// Drops a pairing past its time. Under `mutex_`.
    void lapse(Clock::time_point now);

    std::string hostId_;
    mutable std::mutex mutex_;
    std::optional<Pending> pending_;
    /// Before this no offer is made; cleared when the player answers.
    std::optional<Clock::time_point> nextOffer_;
};

} // namespace opensu::lan
