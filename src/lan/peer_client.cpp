#include "peer_client.hpp"

#include <chrono>
#include <vector>

#include "request_signature.hpp"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

constexpr long forbidden = 403;

} // namespace

std::optional<net::WebClient::Answer> PeerClient::send(std::string_view target,
                                                       const std::vector<std::string>& headers,
                                                       const std::optional<std::string>& post,
                                                       std::size_t largest,
                                                       std::string& error) const {
    const std::string url =
        "http://" + endpoint_.address + ":" + std::to_string(endpoint_.port) + std::string{target};
    const net::WebClient::Guard guard{
        .maxBody = largest, .refusal = [allowed = std::string_view{url}](const std::string& hop) {
            return hop == allowed ? std::string{} : "an openSU sends nowhere else";
        }};
    return web_.request(
        url, headers, guard, error,
        net::WebClient::Sent{.method = {}, .body = post, .user = {}, .password = {}});
}

std::optional<net::WebClient::Answer> PeerClient::post(std::string_view target,
                                                       const std::string& body, std::size_t largest,
                                                       std::string& error) const {
    return send(target, {"Content-Type: application/json"}, body, largest, error);
}

std::optional<net::WebClient::Answer>
PeerClient::call(std::string_view method, std::string_view target, const std::string& body,
                 const Signer& signer, std::size_t largest, std::string& error) const {
    const std::int64_t now = unixSeconds(std::chrono::system_clock::now());
    const std::string nonce =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
    RequestFacts facts{method, target, now, nonce, {}};
    const bool posts = method == "POST";
    const std::string sent =
        posts ? sealBody(signer.secret, facts, Direction::Request, 0, body) : std::string{};
    facts.body = sent;
    const std::string signature = requestSignature(signer.secret, facts);
    const std::vector<std::string> headers{
        "Content-Type: application/octet-stream",
        std::string{peerHeader} + ": " + std::string{signer.selfId},
        std::string{timeHeader} + ": " + std::to_string(now),
        std::string{nonceHeader} + ": " + nonce, std::string{signatureHeader} + ": " + signature};
    std::optional<net::WebClient::Answer> answer =
        send(target, headers, posts ? std::optional<std::string>{sent} : std::nullopt,
             largest + secrets::crypto::sealNonceBytes + secrets::crypto::sealTagBytes, error);
    if (!answer) {
        return std::nullopt;
    }
    std::optional<std::string> opened = openBody(signer.secret, facts, Direction::Answer,
                                                 static_cast<int>(answer->status), answer->body);
    if (!opened) {
        error = answer->status == forbidden ? "it refused: it is not paired with this openSU"
                                            : "the answer is not sealed by the openSU paired with";
        return std::nullopt;
    }
    answer->body = std::move(*opened);
    return answer;
}

} // namespace opensu::lan
