#include "pairing_keys.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

constexpr std::uint32_t codeRange = 1000000;
constexpr std::uint32_t codeHalf = 1000;

std::string joined(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) {
    std::string out{secrets::crypto::asText(a)};
    out += secrets::crypto::asText(b);
    return out;
}

} // namespace

secrets::crypto::Bytes pairingCommitment(std::span<const std::uint8_t> guestKey,
                                         std::span<const std::uint8_t> guestNonce) {
    return secrets::crypto::sha256("opensu-lan-commit\n" + joined(guestKey, guestNonce));
}

std::string pairingCode(const PairingTranscript& transcript) {
    const secrets::crypto::Bytes digest = secrets::crypto::sha256(
        "opensu-lan-code\n" + transcript.guestId + "\n" + transcript.hostId + "\n" +
        joined(transcript.guestKey, transcript.hostKey) +
        joined(transcript.guestNonce, transcript.hostNonce));
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value = (value << 8U) | digest[i];
    }
    value %= codeRange;
    std::array<char, 8> text{};
    std::snprintf(text.data(), text.size(), "%03u %03u", value / codeHalf, value % codeHalf);
    return text.data();
}

secrets::crypto::Bytes pairingSecret(std::span<const std::uint8_t> shared,
                                     const PairingTranscript& transcript) {
    secrets::crypto::Bytes salt = transcript.guestNonce;
    salt.insert(salt.end(), transcript.hostNonce.begin(), transcript.hostNonce.end());
    return secrets::crypto::hkdfSha256(shared, salt,
                                       "opensu-lan-pair\n" + transcript.guestId + "\n" +
                                           transcript.hostId + "\n" +
                                           joined(transcript.guestKey, transcript.hostKey),
                                       secrets::crypto::keyBytes);
}

std::string pairingVerdictSignature(std::span<const std::uint8_t> secret, std::string_view verdict,
                                    std::span<const std::uint8_t> guestNonce) {
    return secrets::hexText(secrets::crypto::hmacSha256(secret, "opensu-lan-verdict\n" +
                                                                    std::string{verdict} + "\n" +
                                                                    secrets::hexText(guestNonce)));
}

StreamKeys streamKeys(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> guestSalt,
                      std::span<const std::uint8_t> hostSalt) {
    secrets::crypto::Bytes salt{guestSalt.begin(), guestSalt.end()};
    salt.insert(salt.end(), hostSalt.begin(), hostSalt.end());
    return StreamKeys{.host = secrets::crypto::hkdfSha256(secret, salt, "opensu-stream\nhost",
                                                          secrets::crypto::keyBytes),
                      .guest = secrets::crypto::hkdfSha256(secret, salt, "opensu-stream\nguest",
                                                           secrets::crypto::keyBytes)};
}

} // namespace opensu::lan
