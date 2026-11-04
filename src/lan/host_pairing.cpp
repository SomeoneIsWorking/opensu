#include "host_pairing.hpp"

#include <utility>

#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::lan {

std::string_view key(Verdict verdict) noexcept {
    switch (verdict) {
    case Verdict::Accepted:
        return "accepted";
    case Verdict::Declined:
        return "declined";
    case Verdict::Waiting:
        break;
    }
    return "waiting";
}

void HostPairing::lapse(Clock::time_point now) {
    if (pending_ && now - pending_->started > patience) {
        lucent::info("lan", "pairing with {} lapsed", pending_->guest.name);
        secrets::wipeBytes(pending_->secret.data(), pending_->secret.size());
        pending_.reset();
    }
}

std::optional<HostPairing::Offer> HostPairing::begin(const Peer& guest, crypto::Bytes commitment,
                                                     Clock::time_point now, std::string& refusal) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (pending_ && pending_->guest.id != guest.id && pending_->verdict == Verdict::Waiting) {
        refusal = "another openSU is pairing with this one";
        return std::nullopt;
    }
    if (nextOffer_ && now < *nextOffer_) {
        refusal = "this openSU was just asked to pair; ask again in a few seconds";
        return std::nullopt;
    }
    nextOffer_ = now + offerSpacing;
    if (pending_) {
        secrets::wipeBytes(pending_->secret.data(), pending_->secret.size());
    }
    pending_.emplace(Pending{.guest = guest,
                             .commitment = std::move(commitment),
                             .key = crypto::KeyPair{},
                             .nonce = crypto::randomBytes(crypto::nonceBytes),
                             .started = now});
    lucent::info("lan", "{} asks to pair", guest.name);
    return Offer{pending_->key.publicKey(), pending_->nonce};
}

bool HostPairing::reveal(std::string_view guestId, const crypto::Bytes& key,
                         const crypto::Bytes& nonce, Clock::time_point now, std::string& refusal) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->guest.id != guestId || pending_->revealed) {
        refusal = "no pairing of that openSU is under way";
        return false;
    }
    if (!crypto::sameBytes(pairingCommitment(key, nonce), pending_->commitment)) {
        refusal = "the key is not the one committed to";
        pending_.reset();
        return false;
    }
    std::optional<crypto::Bytes> shared = pending_->key.agree(key);
    if (!shared) {
        refusal = "that is not an X25519 key";
        pending_.reset();
        return false;
    }
    const PairingTranscript transcript{.guestId = pending_->guest.id,
                                       .hostId = hostId_,
                                       .guestKey = key,
                                       .guestNonce = nonce,
                                       .hostKey = pending_->key.publicKey(),
                                       .hostNonce = pending_->nonce};
    pending_->secret = pairingSecret(*shared, transcript);
    secrets::wipeBytes(shared->data(), shared->size());
    pending_->code = pairingCode(transcript);
    pending_->guestNonce = nonce;
    pending_->revealed = true;
    return true;
}

std::optional<HostPairing::Status> HostPairing::status(std::string_view guestId,
                                                       Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->guest.id != guestId || !pending_->revealed) {
        return std::nullopt;
    }
    Status status{.verdict = pending_->verdict,
                  .signature = pairingVerdictSignature(pending_->secret, key(pending_->verdict),
                                                       pending_->guestNonce)};
    if (pending_->verdict != Verdict::Waiting) {
        // The guest has its answer; the secret stays only with the book.
        secrets::wipeBytes(pending_->secret.data(), pending_->secret.size());
        pending_.reset();
    }
    return status;
}

std::optional<HostPairing::Question> HostPairing::question(Clock::time_point now) const {
    const std::lock_guard lock{mutex_};
    if (!pending_ || !pending_->revealed || pending_->verdict != Verdict::Waiting ||
        now - pending_->started > patience) {
        return std::nullopt;
    }
    return Question{pending_->guest, pending_->code};
}

std::optional<HostPairing::Accepted> HostPairing::accepting(std::string_view guestId,
                                                            Clock::time_point now) const {
    const std::lock_guard lock{mutex_};
    if (!pending_ || pending_->guest.id != guestId || !pending_->revealed ||
        pending_->verdict != Verdict::Waiting || now - pending_->started > patience) {
        return std::nullopt;
    }
    return Accepted{pending_->guest, pending_->secret};
}

void HostPairing::decide(std::string_view guestId, Verdict verdict, Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->guest.id != guestId || !pending_->revealed ||
        pending_->verdict != Verdict::Waiting) {
        return;
    }
    pending_->verdict = verdict;
    nextOffer_.reset();
    lucent::info("lan", "pairing with {}: {}", pending_->guest.name, key(verdict));
}

} // namespace opensu::lan
