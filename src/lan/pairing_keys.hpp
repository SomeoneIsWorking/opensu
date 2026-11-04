// pairing_keys — what two openSUs derive while pairing, the one place each is computed. The guest
// commits to its X25519 key and nonce before it sees the host's, then reveals them; both then
// hold the same transcript, show the same six-digit code from it, and once the player accepts
// keep the same per-peer secret, which never crosses the wire. A man in the middle would have to
// fix his half of each transcript before seeing the other's, so the two codes differ but for a
// one-in-a-million guess.
#pragma once

#include <span>
#include <string>

#include "peer_crypto.hpp"

namespace opensu::lan {

/// Both halves of one pairing.
struct PairingTranscript {
    std::string guestId;
    std::string hostId;
    crypto::Bytes guestKey;
    crypto::Bytes guestNonce;
    crypto::Bytes hostKey;
    crypto::Bytes hostNonce;
};

/// What the guest sends first: SHA-256 over its key and nonce.
[[nodiscard]] crypto::Bytes pairingCommitment(std::span<const std::uint8_t> guestKey,
                                              std::span<const std::uint8_t> guestNonce);

/// The six digits both screens show, as "123 456".
[[nodiscard]] std::string pairingCode(const PairingTranscript& transcript);

/// The per-peer secret both keep, from the X25519 secret they share and the transcript.
[[nodiscard]] crypto::Bytes pairingSecret(std::span<const std::uint8_t> shared,
                                          const PairingTranscript& transcript);

/// The host's word on a pairing ("waiting", "accepted", "declined"), signed with the pairing's
/// secret so the guest keeps it only when it came from the host it paired with.
[[nodiscard]] std::string pairingVerdictSignature(std::span<const std::uint8_t> secret,
                                                  std::string_view verdict,
                                                  std::span<const std::uint8_t> guestNonce);

/// The keys one remote-play session's packets are sealed with, one per sender, so a counter is
/// never used twice under one key.
struct StreamKeys {
    crypto::Bytes host;
    crypto::Bytes guest;
};

/// A session's keys: HKDF over the pairing's secret, salted with the fresh salts both sides sent
/// for it (`crypto::nonceBytes` each).
[[nodiscard]] StreamKeys streamKeys(std::span<const std::uint8_t> secret,
                                    std::span<const std::uint8_t> guestSalt,
                                    std::span<const std::uint8_t> hostSalt);

} // namespace opensu::lan
