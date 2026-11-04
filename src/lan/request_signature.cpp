#include "request_signature.hpp"

#include <span>

#include "peer_crypto.hpp"
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
    message += secrets::hexText(crypto::sha256(facts.body));
    return secrets::hexText(crypto::hmacSha256(secret, message));
}

std::string answerSignature(std::span<const std::uint8_t> secret, std::string_view nonce,
                            int status, std::string_view body) {
    std::string message = "opensu-lan-answer\n";
    message += nonce;
    message += '\n';
    message += std::to_string(status);
    message += '\n';
    message += secrets::hexText(crypto::sha256(body));
    return secrets::hexText(crypto::hmacSha256(secret, message));
}

bool signatureMatches(std::string_view expected, std::string_view given) noexcept {
    return crypto::sameBytes(
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
