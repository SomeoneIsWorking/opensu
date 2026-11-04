#include "lan_listener.hpp"

#include <algorithm>
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
constexpr std::string_view savesPrefix = "/lan/saves/";
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
        (request.method == "GET" &&
         (path == "/lan/library" || path == wire::romListRoute || path.starts_with(coverPrefix))) ||
        (request.method == "POST" &&
         (path == "/lan/unpair" || path == wire::playRoute || path.starts_with(savesPrefix) ||
          path == wire::romFileRoute || path == wire::romChunkRoute));
    if (!peerPath) {
        return refused(notFound, "Not Found", "no such route");
    }
    const std::optional<Signed> verified = verify(request);
    if (!verified) {
        return refused(forbidden, "Forbidden", "pair with this openSU first");
    }
    const RequestFacts facts{request.method, request.target, verified->time, verified->nonce,
                             request.body};
    Response answer;
    if (request.method != "POST") {
        answer = peerRoute(request, *verified);
    } else if (const std::optional<std::string> body =
                   openBody(verified->secret, facts, Direction::Request, 0, request.body)) {
        if (path.starts_with(savesPrefix)) {
            answer = savesRoute(path, *body, *verified);
        } else if (path == wire::playRoute) {
            answer = playRoute(*body, *verified);
        } else if (path == wire::romFileRoute || path == wire::romChunkRoute) {
            answer = romsRoute(path, *body, *verified);
        } else {
            answer = peerRoute(request, *verified);
        }
    } else {
        lucent::info("lan", "refused {} {} from {}: its body does not open", request.method, path,
                     verified->peer.name);
        answer = refused(forbidden, "Forbidden", "the body is not sealed for this openSU");
    }
    answer.body = sealBody(verified->secret, facts, Direction::Answer, answer.status, answer.body);
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
    return Signed{*known, std::move(*secret), std::string{*nonce}, *sent};
}

Response LanListener::peerRoute(const Request& request, const Signed& verified) {
    const std::string_view path = request.path();
    if (path == "/lan/library") {
        return Response::json(ok, "OK", wire::write(sources_.library.listing()));
    }
    if (path == wire::romListRoute) {
        return Response::json(ok, "OK", wire::write(sources_.roms.listing()));
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

Response LanListener::savesRoute(std::string_view path, std::string_view body,
                                 const Signed& verified) {
    if (!verified.peer.sharesSaves) {
        lucent::info("lan", "refused {} from {}: saves are not shared with it", path,
                     verified.peer.name);
        return refused(forbidden, "Forbidden", "saves are not shared with this openSU");
    }
    const std::optional<wire::SaveAsk> ask = wire::readSaveAsk(body);
    if (!ask) {
        return refused(badRequest, "Bad Request", "not a game's saves");
    }
    const library::saves::SaveKey key{ask->system, ask->game};
    if (path == wire::saveStatusRoute) {
        const std::optional<wire::SaveStatus> status = sources_.saves.statusOf(key);
        if (!status) {
            return refused(notFound, "Not Found", "no game here has those saves");
        }
        return Response::json(ok, "OK", wire::write(*status));
    }
    if (path == wire::saveGetRoute) {
        std::string error;
        std::optional<std::string> bundle = sources_.saves.sendable(key, error);
        if (!bundle) {
            return refused(notFound, "Not Found", error);
        }
        lucent::info("lan", "sending {} the saves of {} {}", verified.peer.name, key.system,
                     key.game);
        return Response::binary(ok, "OK", "application/octet-stream", std::move(*bundle));
    }
    if (path == wire::saveNewsRoute) {
        sources_.saves.heard(verified.peer, *ask);
        return Response::json(ok, "OK", "{}");
    }
    if (path == wire::saveSyncedRoute) {
        sources_.saves.syncedWith(verified.peer, *ask);
        return Response::json(ok, "OK", "{}");
    }
    return refused(notFound, "Not Found", "no such route");
}

Response LanListener::romsRoute(std::string_view path, std::string_view body,
                                const Signed& verified) {
    const std::optional<wire::RomAsk> ask = wire::readRomAsk(body);
    if (!ask) {
        lucent::info("lan", "refused {} from {}: not a file inside a ROM or BIOS folder", path,
                     verified.peer.name);
        return refused(forbidden, "Forbidden",
                       "that path is outside the folders this openSU lists");
    }
    if (path == wire::romFileRoute) {
        const std::optional<wire::RomFacts> facts = sources_.roms.factsOf(*ask);
        if (!facts) {
            lucent::info("lan", "refused {} the file {}: not in a folder this openSU lists",
                         verified.peer.name, ask->path);
            return refused(forbidden, "Forbidden", "no ROM folder here lists that file");
        }
        return Response::json(ok, "OK", wire::write(*facts));
    }
    std::string error;
    std::optional<std::string> chunk = sources_.roms.chunkOf(*ask, error);
    if (!chunk) {
        lucent::info("lan", "refused {} a chunk of {}: {}", verified.peer.name, ask->path, error);
        return refused(forbidden, "Forbidden", error);
    }
    return Response::binary(ok, "OK", "application/octet-stream", std::move(*chunk));
}

Response LanListener::playRoute(std::string_view body, const Signed& verified) {
    const std::optional<wire::PlayRequest> asked = wire::readPlayRequest(body);
    if (!asked) {
        return refused(badRequest, "Bad Request", "not a request to play");
    }
    const std::vector<wire::ListedGame> listed = sources_.library.listing();
    const auto game = std::ranges::find(listed, asked->game, &wire::ListedGame::id);
    if (game == listed.end()) {
        return refused(notFound, "Not Found", "no game here has that id");
    }
    const crypto::Bytes salt = crypto::randomBytes(crypto::nonceBytes);
    StreamKeys keys = streamKeys(verified.secret, asked->salt, salt);
    std::string refusal = "this openSU does not offer remote play";
    const std::optional<std::uint16_t> port =
        sources_.play ? sources_.play(verified.peer, game->id, keys, refusal) : std::nullopt;
    secrets::wipeBytes(keys.host.data(), keys.host.size());
    secrets::wipeBytes(keys.guest.data(), keys.guest.size());
    if (!port) {
        lucent::info("lan", "refused {} remote play of {}: {}", verified.peer.name, game->title,
                     refusal);
        return refused(conflict, "Conflict", refusal);
    }
    lucent::info("lan", "{} plays {} here over remote play, stream on UDP port {}",
                 verified.peer.name, game->title, *port);
    return Response::json(ok, "OK", wire::write(wire::PlayAnswer{*port, salt}));
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
