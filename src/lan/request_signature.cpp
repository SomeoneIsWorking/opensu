#include "request_signature.hpp"

#include <span>

#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {

std::string requestSignature(std::span<const std::uint8_t> secret, const RequestFacts& facts) {
    std::string message = "opensu-lan-request\n";
    message += facts.method;
    message += '\n';
    message += facts.target;
    message += '\n';
    message += std::to_string(facts.time);
    message += '\n';
    message += facts.nonce;
    message += '\n';
    message += secrets::hexText(secrets::crypto::sha256(facts.body));
    return secrets::hexText(secrets::crypto::hmacSha256(secret, message));
}

namespace {

/// The key and the authenticated data a body going `direction` is sealed with.
struct Sealing {
    secrets::crypto::Bytes key;
    std::string aad;
};

Sealing sealingOf(std::span<const std::uint8_t> secret, const RequestFacts& facts,
                  Direction direction, int status) {
    const std::string way = direction == Direction::Request ? "request" : "answer";
    std::string info = "opensu-lan-seal\n" + way + "\n";
    info += facts.method;
    info += '\n';
    info += facts.target;
    info += '\n';
    info += std::to_string(facts.time);
    const std::optional<secrets::crypto::Bytes> salt = secrets::hexBytes(facts.nonce);
    return Sealing{secrets::crypto::hkdfSha256(secret, salt ? *salt : secrets::crypto::Bytes{},
                                               info, secrets::crypto::keyBytes),
                   info + "\n" + std::to_string(status)};
}

} // namespace

std::string sealBody(std::span<const std::uint8_t> secret, const RequestFacts& facts,
                     Direction direction, int status, std::string_view plain) {
    Sealing sealing = sealingOf(secret, facts, direction, status);
    // A fresh nonce as well as a fresh key: a request replayed after the host restarted, when its
    // replay guard is empty, would otherwise seal a second answer under one key and nonce.
    const secrets::crypto::Bytes nonce =
        secrets::crypto::randomBytes(secrets::crypto::sealNonceBytes);
    std::string sealed{nonce.begin(), nonce.end()};
    sealed += secrets::crypto::seal(sealing.key, nonce, sealing.aad, plain);
    secrets::wipeBytes(sealing.key.data(), sealing.key.size());
    return sealed;
}

std::optional<std::string> openBody(std::span<const std::uint8_t> secret, const RequestFacts& facts,
                                    Direction direction, int status, std::string_view sealed) {
    if (sealed.size() < secrets::crypto::sealNonceBytes) {
        return std::nullopt;
    }
    Sealing sealing = sealingOf(secret, facts, direction, status);
    const secrets::crypto::Bytes nonce{sealed.begin(),
                                       sealed.begin() + secrets::crypto::sealNonceBytes};
    std::optional<std::string> plain = secrets::crypto::open(
        sealing.key, nonce, sealing.aad, sealed.substr(secrets::crypto::sealNonceBytes));
    secrets::wipeBytes(sealing.key.data(), sealing.key.size());
    return plain;
}

bool signatureMatches(std::string_view expected, std::string_view given) noexcept {
    return secrets::crypto::sameBytes(
        std::span{reinterpret_cast<const std::uint8_t*>(expected.data()), expected.size()},
        std::span{reinterpret_cast<const std::uint8_t*>(given.data()), given.size()});
}

std::int64_t unixSeconds(std::chrono::system_clock::time_point at) {
    return std::chrono::duration_cast<std::chrono::seconds>(at.time_since_epoch()).count();
}

bool ReplayGuard::admit(const std::string& peer, std::string_view nonce, std::int64_t time,
                        std::int64_t now) {
    const std::int64_t slack = clockSlack.count();
    if (time < now - slack || time > now + slack) {
        return false;
    }
    const std::lock_guard lock{mutex_};
    std::erase_if(heard_, [now, slack](const auto& entry) {
        return entry.second < now - slack;
    });
    std::string key = peer;
    key += ' ';
    key += nonce;
    return heard_.emplace(std::move(key), time).second;
}

} // namespace opensu::lan
