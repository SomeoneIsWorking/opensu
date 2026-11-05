#include "peer_save_target.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"
#include "peer_client.hpp"
#include "secrets/secret.hpp"
#include "wire.hpp"

namespace opensu::lan {
namespace {

using library::saves::SaveKey;

constexpr long ok = 200;
constexpr long forbidden = 403;
constexpr long notFound = 404;
/// A status, a notice or a record is a few hundred bytes.
constexpr std::size_t largestNote = std::size_t{16} * 1024;

wire::SaveAsk askFor(const SaveKey& key) {
    return wire::SaveAsk{.system = key.system,
                         .game = key.game,
                         .profile = key.profile,
                         .picked = false,
                         .hash = {},
                         .counter = 0};
}

std::string describe(const net::Result<net::WebClient::Answer>& answer) {
    return answer ? "HTTP " + std::to_string(answer->status) : answer.error();
}

} // namespace

PeerSaveTarget::PeerSaveTarget(Peer peer, const PeerBook& book,
                               const profiles::ProfileBook& profiles, const net::WebClient& web,
                               std::function<std::vector<Sighting>()> online)
    : peer_{std::move(peer)}, book_{book}, profiles_{profiles}, web_{web},
      online_{std::move(online)} {
}

bool PeerSaveTarget::reachable() const {
    const std::vector<Sighting> seen = online_();
    return std::ranges::find(seen, peer_.id, &Sighting::id) != seen.end();
}

net::Result<net::WebClient::Answer> PeerSaveTarget::call(const std::stop_token& stop,
                                                         std::string_view route,
                                                         const std::string& body,
                                                         std::size_t largest) const {
    const std::vector<Sighting> seen = online_();
    const auto sighting = std::ranges::find(seen, peer_.id, &Sighting::id);
    if (sighting == seen.end()) {
        return net::Failure{peer_.name + " is offline"};
    }
    std::optional<secrets::crypto::Bytes> secret = book_.secretOf(peer_.id);
    if (!secret) {
        return net::Failure{peer_.name + " is no longer paired"};
    }
    const PeerClient client{web_, Endpoint{sighting->address, sighting->port}, stop};
    net::Result<net::WebClient::Answer> answer =
        client.call("POST", route, body, PeerClient::Signer{book_.id(), *secret}, largest);
    secrets::wipeBytes(secret->data(), secret->size());
    return answer;
}

save_sync::SaveTarget::Held PeerSaveTarget::status(const std::stop_token& /*stop*/,
                                                   const SaveKey& key) {
    if (!profiles_.linked(key.profile, peer_.id)) {
        return Held{.kind = Held::Kind::NotShared, .status = {}, .problem = {}};
    }
    const net::Result<net::WebClient::Answer> answer =
        call(std::stop_token{}, wire::saveStatusRoute, wire::write(askFor(key)), largestNote);
    if (answer && answer->status == forbidden) {
        return Held{.kind = Held::Kind::NotShared, .status = {}, .problem = {}};
    }
    if (answer && answer->status == notFound) {
        return Held{.kind = Held::Kind::NoGame, .status = {}, .problem = {}};
    }
    std::optional<save_sync::SaveStatus> theirs;
    if (answer && answer->status == ok) {
        theirs = save_sync::readSaveStatus(answer->body);
    }
    if (!theirs) {
        return Held{.kind = Held::Kind::Failed,
                    .status = {},
                    .problem = peer_.name + " cannot be asked: " + describe(answer)};
    }
    return Held{.kind = Held::Kind::Saves, .status = std::move(*theirs), .problem = {}};
}

std::optional<save_sync::SaveBundle> PeerSaveTarget::fetch(const std::stop_token& /*stop*/,
                                                           const SaveKey& key, std::string& error) {
    const net::Result<net::WebClient::Answer> answer = call(
        std::stop_token{}, wire::saveGetRoute, wire::write(askFor(key)), save_sync::largestBundle);
    if (!answer || answer->status != ok) {
        error = describe(answer);
        return std::nullopt;
    }
    std::optional<save_sync::SaveBundle> bundle = save_sync::readBundle(answer->body);
    if (!bundle) {
        error = "what it sent is not a game's saves";
    }
    return bundle;
}

save_sync::SaveTarget::Given PeerSaveTarget::give(const std::stop_token& /*stop*/,
                                                  const SaveKey& key,
                                                  const save_sync::SaveBundle& /*bundle*/,
                                                  const save_sync::SaveStatus& /*status*/,
                                                  bool picked, std::string& error) {
    wire::SaveAsk news = askFor(key);
    news.picked = picked;
    const net::Result<net::WebClient::Answer> answer =
        call(std::stop_token{}, wire::saveNewsRoute, wire::write(news), largestNote);
    if (!answer || answer->status != ok) {
        error = peer_.name + " was not told of the newer saves: " + describe(answer);
        return Given::Failed;
    }
    return Given::Told;
}

void PeerSaveTarget::took(const std::stop_token& /*stop*/, const SaveKey& key,
                          const save_sync::SaveStatus& status) {
    wire::SaveAsk record = askFor(key);
    record.hash = status.hash;
    record.counter = status.counter;
    const net::Result<net::WebClient::Answer> answer =
        call(std::stop_token{}, wire::saveSyncedRoute, wire::write(record), largestNote);
    if (!answer || answer->status != ok) {
        lucent::warn("saves", "{} was not told its saves were taken: {}", peer_.name,
                     describe(answer));
    }
}

} // namespace opensu::lan
