#include "guest_pairing.hpp"

#include <chrono>
#include <condition_variable>
#include <utility>

#include "host_pairing.hpp"
#include "lucent/log.h"
#include "pairing_keys.hpp"
#include "peer_client.hpp"
#include "request_signature.hpp"
#include "secrets/secret.hpp"
#include "wire.hpp"

namespace opensu::lan {
namespace {

constexpr long ok = 200;
/// A pairing answer is a few hundred bytes.
constexpr std::size_t largestAnswer = std::size_t{16} * 1024;
constexpr std::chrono::seconds pollEvery{1};

/// Waits `span`, or less when `stop` is asked for; false when it was.
bool pause(const std::stop_token& stop, std::chrono::seconds span) {
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock{mutex};
    return !wake.wait_for(lock, stop, span, [] {
        return false;
    }) && !stop.stop_requested();
}

} // namespace

GuestPairing::GuestPairing(const net::WebClient& web, PeerBook& book, Peer self, Sighting host)
    : web_{web}, book_{book}, self_{std::move(self)}, host_{std::move(host)},
      state_{State{.phase = Phase::Asking, .hostName = host_.name, .code = {}, .problem = {}}} {
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
}

GuestPairing::~GuestPairing() {
    worker_.request_stop();
}

GuestPairing::State GuestPairing::state() const {
    const std::lock_guard lock{mutex_};
    return state_;
}

void GuestPairing::cancel() {
    worker_.request_stop();
}

bool GuestPairing::done() const {
    const std::lock_guard lock{mutex_};
    return state_.phase != Phase::Asking && state_.phase != Phase::Waiting;
}

void GuestPairing::enter(Phase phase, std::string problem) {
    const std::lock_guard lock{mutex_};
    state_.phase = phase;
    state_.problem = std::move(problem);
}

void GuestPairing::run(const std::stop_token& stop) {
    std::string problem;
    Phase ended = Phase::Failed;
    try {
        ended = pair(stop, problem);
    } catch (const std::exception& failure) {
        problem = failure.what();
    }
    if (stop.stop_requested() && ended != Phase::Paired) {
        ended = Phase::Cancelled;
    }
    if (ended == Phase::Failed) {
        lucent::warn("lan", "pairing with {} failed: {}", host_.name, problem);
    }
    enter(ended, std::move(problem));
}

GuestPairing::Phase GuestPairing::pair(const std::stop_token& stop, std::string& problem) {
    const PeerClient client{web_, Endpoint{host_.address, host_.port}, std::stop_token{}};
    const secrets::crypto::KeyPair ours;
    const secrets::crypto::Bytes nonce = secrets::crypto::randomBytes(secrets::crypto::nonceBytes);
    net::Result<net::WebClient::Answer> answer =
        client.post("/lan/pair",
                    wire::write(wire::PairRequest{self_.id, self_.name,
                                                  pairingCommitment(ours.publicKey(), nonce)}),
                    largestAnswer);
    if (!answer) {
        problem = answer.error();
        return Phase::Failed;
    }
    if (answer->status != ok) {
        problem = host_.name + " refused: " + answer->body;
        return Phase::Failed;
    }
    const std::optional<wire::PairOffer> offer = wire::readPairOffer(answer->body);
    if (!offer) {
        problem = host_.name + " did not answer as an openSU";
        return Phase::Failed;
    }
    std::optional<secrets::crypto::Bytes> shared = ours.agree(offer->key);
    if (!shared) {
        problem = host_.name + " sent a key that is not one";
        return Phase::Failed;
    }
    const PairingTranscript transcript{.guestId = self_.id,
                                       .hostId = host_.id,
                                       .guestKey = ours.publicKey(),
                                       .guestNonce = nonce,
                                       .hostKey = offer->key,
                                       .hostNonce = offer->nonce};
    secrets::crypto::Bytes secret = pairingSecret(*shared, transcript);
    secrets::wipeBytes(shared->data(), shared->size());
    answer = client.post("/lan/pair/reveal",
                         wire::write(wire::PairReveal{self_.id, ours.publicKey(), nonce}),
                         largestAnswer);
    if (!answer || answer->status != ok) {
        problem = answer ? host_.name + " refused: " + answer->body : answer.error();
        secrets::wipeBytes(secret.data(), secret.size());
        return Phase::Failed;
    }
    {
        const std::lock_guard lock{mutex_};
        state_.code = pairingCode(transcript);
        state_.phase = Phase::Waiting;
    }
    lucent::info("lan", "pairing with {}: waiting for the player there", host_.name);
    const auto until = std::chrono::steady_clock::now() + patience;
    Phase ended = Phase::Failed;
    problem = host_.name + " did not answer in time";
    while (std::chrono::steady_clock::now() < until && pause(stop, pollEvery)) {
        answer =
            client.post("/lan/pair/status", wire::write(wire::PairPoll{self_.id}), largestAnswer);
        if (!answer || answer->status != ok) {
            continue;
        }
        const std::optional<wire::PairVerdict> verdict = wire::readPairVerdict(answer->body);
        if (!verdict || verdict->verdict == key(Verdict::Waiting)) {
            continue;
        }
        if (!signatureMatches(pairingVerdictSignature(secret, verdict->verdict, nonce),
                              verdict->signature)) {
            problem = "the answer did not come from " + host_.name;
            break;
        }
        if (verdict->verdict == key(Verdict::Declined)) {
            problem.clear();
            ended = Phase::Declined;
            break;
        }
        if (verdict->verdict == key(Verdict::Accepted)) {
            if (book_.remember(Peer{host_.id, host_.name}, secret, problem)) {
                lucent::info("lan", "paired with {}", host_.name);
                problem.clear();
                ended = Phase::Paired;
            }
            break;
        }
    }
    secrets::wipeBytes(secret.data(), secret.size());
    return ended;
}

} // namespace opensu::lan
