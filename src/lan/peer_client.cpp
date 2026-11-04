#include "peer_client.hpp"

#include <chrono>
#include <vector>

#include "peer_crypto.hpp"
#include "request_signature.hpp"
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
    const net::WebClient::Guard guard{.maxBody = largest, .refusal = [url](const std::string& hop) {
                                          return hop == url ? std::string{}
                                                            : "an openSU sends nowhere else";
                                      }};
    return web_.request(url, headers, guard, error, post);
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
    const std::string nonce = secrets::hexText(crypto::randomBytes(crypto::nonceBytes));
    const std::string signature =
        requestSignature(signer.secret, RequestFacts{method, target, now, nonce, body});
    const std::vector<std::string> headers{
        "Content-Type: application/json",
        std::string{peerHeader} + ": " + std::string{signer.selfId},
        std::string{timeHeader} + ": " + std::to_string(now),
        std::string{nonceHeader} + ": " + nonce, std::string{signatureHeader} + ": " + signature};
    std::optional<net::WebClient::Answer> answer =
        send(target, headers, method == "POST" ? std::optional<std::string>{body} : std::nullopt,
             largest, error);
    if (!answer) {
        return std::nullopt;
    }
    const std::string expected =
        answerSignature(signer.secret, nonce, static_cast<int>(answer->status), answer->body);
    if (!signatureMatches(expected, answer->header(signatureHeader))) {
        error = answer->status == forbidden ? "it refused: it is not paired with this openSU"
                                            : "the answer is not signed by the openSU paired with";
        return std::nullopt;
    }
    return answer;
}

} // namespace opensu::lan
