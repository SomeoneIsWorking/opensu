#include "lan_listener.hpp"

#include <charconv>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "lucent/log.h"
#include "secrets/secret.hpp"
#include "wire.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using lucent::http::Request;
using lucent::http::Response;

constexpr int ok = 200;
constexpr int badRequest = 400;
constexpr int forbidden = 403;
constexpr int notFound = 404;
constexpr int conflict = 409;
constexpr int tooLarge = 413;
constexpr std::string_view coverPrefix = "/lan/cover/";
/// A cover larger than this is not sent.
constexpr std::uintmax_t largestCover = std::uintmax_t{16} * 1024 * 1024;
/// A pairing or unpairing body is a few hundred bytes.
constexpr std::size_t largestBody = std::size_t{16} * 1024;

Response refused(int status, std::string reason, std::string why) {
    return Response::text(status, std::move(reason), std::move(why) + "\n");
}

std::string_view contentTypeOf(const fs::path& file) {
    const std::string extension = file.extension().string();
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".webp") {
        return "image/webp";
    }
    return "image/jpeg";
}

std::optional<std::int64_t> numberOf(std::string_view text) {
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace

LanListener::LanListener(Sources sources, std::uint16_t port)
    : sources_{std::move(sources)},
      server_{lucent::http::ServerOptions{.port = port,
                                          .listen_scope = lucent::http::ListenScope::LocalNetwork,
                                          .max_body_bytes = largestBody},
              [this](const Request& request) {
                  return handle(request);
              }} {
}

LanListener::~LanListener() {
    server_.stop();
}

bool LanListener::start() {
    return server_.start();
}

void LanListener::stop() {
    server_.stop();
}

std::uint16_t LanListener::port() const noexcept {
    return server_.port();
}

Response LanListener::handle(const Request& request) {
    const std::string_view path = request.path();
    if (request.method == "GET" && path == "/lan/hello") {
        return Response::json(ok, "OK",
                              wire::write(wire::Hello{sources_.book.id(), sources_.name()}));
    }
    if (request.method == "POST" && path == "/lan/pair") {
        return pair(request);
    }
    if (request.method == "POST" && path == "/lan/pair/reveal") {
        return reveal(request);
    }
    if (request.method == "POST" && path == "/lan/pair/status") {
        return pairStatus(request);
    }
    const bool peerPath =
        (request.method == "GET" && (path == "/lan/library" || path.starts_with(coverPrefix))) ||
        (request.method == "POST" && path == "/lan/unpair");
    if (!peerPath) {
        return refused(notFound, "Not Found", "no such route");
    }
    const std::optional<Signed> verified = verify(request);
    if (!verified) {
        return refused(forbidden, "Forbidden", "pair with this openSU first");
    }
    Response answer = peerRoute(request, *verified);
    answer.headers.push_back(
        {std::string{signatureHeader},
         answerSignature(verified->secret, verified->nonce, answer.status, answer.body)});
    return answer;
}

std::optional<LanListener::Signed> LanListener::verify(const Request& request) {
    const std::optional<std::string_view> peer = request.header(peerHeader);
    const std::optional<std::string_view> time = request.header(timeHeader);
    const std::optional<std::string_view> nonce = request.header(nonceHeader);
    const std::optional<std::string_view> signature = request.header(signatureHeader);
    if (!peer || !time || !nonce || !signature || !isPeerId(*peer) ||
        nonce->size() != crypto::nonceBytes * 2 || !secrets::isLowerHex(*nonce)) {
        return std::nullopt;
    }
    const std::optional<std::int64_t> sent = numberOf(*time);
    const std::optional<Peer> known = sources_.book.find(*peer);
    std::optional<crypto::Bytes> secret = known ? sources_.book.secretOf(*peer) : std::nullopt;
    if (!sent || !secret) {
        lucent::info("lan", "refused {} {} from an openSU not paired with this one", request.method,
                     request.path());
        return std::nullopt;
    }
    const std::string expected = requestSignature(
        *secret, RequestFacts{request.method, request.target, *sent, *nonce, request.body});
    if (!signatureMatches(expected, *signature) ||
        !replay_.admit(known->id, *nonce, *sent, unixSeconds(std::chrono::system_clock::now()))) {
        lucent::info("lan", "refused {} {} from {}: unsigned, late or repeated", request.method,
                     request.path(), known->name);
        secrets::wipeBytes(secret->data(), secret->size());
        return std::nullopt;
    }
    return Signed{*known, std::move(*secret), std::string{*nonce}};
}

Response LanListener::peerRoute(const Request& request, const Signed& verified) {
    const std::string_view path = request.path();
    if (path == "/lan/library") {
        return Response::json(ok, "OK", wire::write(sources_.library.listing()));
    }
    if (path == "/lan/unpair") {
        std::string error;
        if (!sources_.book.forget(verified.peer.id, error)) {
            lucent::warn("lan", "{} unpaired, but cannot be forgotten: {}", verified.peer.name,
                         error);
            return refused(conflict, "Conflict", "it could not be forgotten");
        }
        lucent::info("lan", "{} unpaired from this openSU", verified.peer.name);
        sources_.unpaired(verified.peer);
        return Response::json(ok, "OK", "{}");
    }
    const std::optional<fs::path> cover = sources_.library.cover(path.substr(coverPrefix.size()));
    if (!cover) {
        return refused(notFound, "Not Found", "no such cover");
    }
    std::error_code ec;
    if (fs::file_size(*cover, ec) > largestCover || ec) {
        return refused(tooLarge, "Payload Too Large", "the cover is too large to send");
    }
    std::ifstream in{*cover, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (!in.good() && !in.eof()) {
        return refused(notFound, "Not Found", "the cover cannot be read");
    }
    return Response::binary(ok, "OK", std::string{contentTypeOf(*cover)}, std::move(bytes));
}

Response LanListener::pair(const Request& request) {
    const std::optional<wire::PairRequest> asked = wire::readPairRequest(request.body);
    if (!asked || asked->id == sources_.book.id()) {
        return refused(badRequest, "Bad Request", "not a pairing request");
    }
    std::string refusal;
    std::optional<HostPairing::Offer> offer = sources_.pairing.begin(
        Peer{asked->id, asked->name}, asked->commitment, HostPairing::Clock::now(), refusal);
    if (!offer) {
        return refused(conflict, "Conflict", refusal);
    }
    return Response::json(ok, "OK", wire::write(wire::PairOffer{offer->key, offer->nonce}));
}

Response LanListener::reveal(const Request& request) {
    const std::optional<wire::PairReveal> shown = wire::readPairReveal(request.body);
    if (!shown) {
        return refused(badRequest, "Bad Request", "not a pairing reveal");
    }
    std::string refusal;
    if (!sources_.pairing.reveal(shown->id, shown->key, shown->nonce, HostPairing::Clock::now(),
                                 refusal)) {
        return refused(conflict, "Conflict", refusal);
    }
    return Response::json(ok, "OK", "{}");
}

Response LanListener::pairStatus(const Request& request) {
    const std::optional<wire::PairPoll> poll = wire::readPairPoll(request.body);
    if (!poll) {
        return refused(badRequest, "Bad Request", "not a pairing poll");
    }
    const std::optional<HostPairing::Status> status =
        sources_.pairing.status(poll->id, HostPairing::Clock::now());
    if (!status) {
        return refused(notFound, "Not Found", "no pairing of that openSU is under way");
    }
    return Response::json(
        ok, "OK",
        wire::write(wire::PairVerdict{std::string{key(status->verdict)}, status->signature}));
}

} // namespace opensu::lan
