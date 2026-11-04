#include "lan_node.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <set>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "pairing_keys.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;

constexpr long ok = 200;
constexpr std::size_t largestListing = std::size_t{8} * 1024 * 1024;
constexpr std::size_t largestCover = std::size_t{16} * 1024 * 1024;
constexpr std::size_t largestNotice = std::size_t{16} * 1024;
/// How often the worker looks at who is announced.
constexpr std::chrono::seconds lookEvery{2};
/// How often the paired peers' games are asked for again while nothing changes.
constexpr std::chrono::seconds refetchEvery{60};
constexpr std::array<std::string_view, 3> coverExtensions{".png", ".jpg", ".webp"};

std::string_view extensionOf(std::string_view contentType) {
    if (contentType.starts_with("image/png")) {
        return ".png";
    }
    if (contentType.starts_with("image/webp")) {
        return ".webp";
    }
    return ".jpg";
}

} // namespace

LanNode::LanNode(Setup setup)
    : coverCache_{std::move(setup.coverCache)}, remote_{std::move(setup.remote)},
      changed_{std::move(setup.changed)},
      book_{std::move(setup.bookFile), std::move(setup.secrets)}, name_{std::move(setup.name)},
      saves_{std::move(setup.saves), book_, web_,
             [this] {
                 return sightings();
             }},
      listener_{LanListener::Sources{.book = book_,
                                     .pairing = pairing_,
                                     .library = library_,
                                     .saves = saves_,
                                     .name =
                                         [this] {
                                             return name_;
                                         },
                                     .unpaired =
                                         [this](const Peer& /*peer*/) {
                                             wake();
                                         },
                                     .play = std::move(setup.play)},
                setup.port} {
    if (!listener_.start()) {
        problem_ = setup.port == 0 ? std::string{"no port can be had to listen on"}
                                   : "port " + std::to_string(setup.port) + " is taken";
        lucent::warn("lan", "the LAN listener cannot start: {}", problem_);
    } else {
        lucent::info("lan", "listening for other openSUs on port {}", listener_.port());
        std::string error;
        discovery_ = setup.discovery(Announcement{book_.id(), name_, listener_.port()}, error);
        if (!discovery_) {
            problem_ = error;
            lucent::warn("lan", "this openSU cannot be found on the LAN: {}", problem_);
        }
    }
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        work(stop);
    }};
}

LanNode::~LanNode() {
    worker_.request_stop();
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    saves_.stop();
    asking_.reset();
    discovery_.reset();
    listener_.stop();
}

LanNode::Status LanNode::status() const {
    return Status{.name = discovery_ ? discovery_->announcedName() : name_,
                  .port = listener_.port(),
                  .problem = problem_};
}

std::vector<Sighting> LanNode::sightings() const {
    return discovery_ ? discovery_->sightings() : std::vector<Sighting>{};
}

bool LanNode::answer(const std::string& guestId, bool accept, std::string& error) {
    const auto now = HostPairing::Clock::now();
    if (!accept) {
        pairing_.decide(guestId, Verdict::Declined, now);
        return true;
    }
    std::optional<HostPairing::Accepted> accepted = pairing_.accepting(guestId, now);
    if (!accepted) {
        error = "the pairing lapsed; ask again from the other openSU";
        return false;
    }
    const bool kept = book_.remember(accepted->guest, accepted->secret, error);
    secrets::wipeBytes(accepted->secret.data(), accepted->secret.size());
    pairing_.decide(guestId, kept ? Verdict::Accepted : Verdict::Declined, now);
    if (kept) {
        wake();
    }
    return kept;
}

bool LanNode::pair(const std::string& peerId, std::string& refusal) {
    if (asking_ && !asking_->done()) {
        refusal = "already pairing with " + asking_->host().name;
        return false;
    }
    const std::vector<Sighting> seen = sightings();
    const auto host = std::ranges::find(seen, peerId, &Sighting::id);
    if (host == seen.end()) {
        refusal = "that openSU is no longer on the LAN";
        return false;
    }
    asking_ = std::make_unique<GuestPairing>(web_, book_, Peer{book_.id(), status().name}, *host);
    return true;
}

std::optional<GuestPairing::State> LanNode::asking() const {
    if (!asking_) {
        return std::nullopt;
    }
    return asking_->state();
}

void LanNode::endAsking() {
    asking_.reset();
    wake();
}

std::optional<LanNode::PlayOffer> LanNode::askToPlay(const std::string& hostId,
                                                     const std::string& gameId,
                                                     std::string& error) const {
    const std::vector<Sighting> seen = sightings();
    const auto host = std::ranges::find(seen, hostId, &Sighting::id);
    std::optional<crypto::Bytes> secret = book_.secretOf(hostId);
    if (host == seen.end() || !secret) {
        error = host == seen.end() ? "that openSU is not on the LAN now"
                                   : "that openSU is not paired with this one";
        return std::nullopt;
    }
    const crypto::Bytes salt = crypto::randomBytes(crypto::nonceBytes);
    const PeerClient client{web_, Endpoint{host->address, host->port}};
    const std::optional<net::WebClient::Answer> answer =
        client.call("POST", wire::playRoute, wire::write(wire::PlayRequest{gameId, salt}),
                    PeerClient::Signer{book_.id(), *secret}, largestNotice, error);
    std::optional<wire::PlayAnswer> offered;
    if (answer && answer->status == ok) {
        offered = wire::readPlayAnswer(answer->body);
    } else if (answer) {
        error = answer->body.empty() ? "HTTP " + std::to_string(answer->status) : answer->body;
        while (!error.empty() && error.back() == '\n') {
            error.pop_back();
        }
    }
    std::optional<PlayOffer> offer;
    if (offered) {
        offer = PlayOffer{Endpoint{host->address, offered->port},
                          streamKeys(*secret, salt, offered->salt)};
    } else if (answer && answer->status == ok) {
        error = "its answer is not an offer to stream";
    }
    secrets::wipeBytes(secret->data(), secret->size());
    return offer;
}

bool LanNode::forget(const std::string& peerId, std::string& error) {
    std::optional<crypto::Bytes> secret = book_.secretOf(peerId);
    if (!book_.forget(peerId, error)) {
        if (secret) {
            secrets::wipeBytes(secret->data(), secret->size());
        }
        return false;
    }
    const std::vector<Sighting> seen = sightings();
    const auto peer = std::ranges::find(seen, peerId, &Sighting::id);
    {
        const std::lock_guard lock{mutex_};
        if (secret && peer != seen.end()) {
            notices_.push_back(Notice{Endpoint{peer->address, peer->port}, std::move(*secret)});
        }
        woken_ = true;
    }
    wake_.notify_all();
    return true;
}

void LanNode::wake() {
    {
        const std::lock_guard lock{mutex_};
        woken_ = true;
    }
    wake_.notify_all();
}

void LanNode::work(const std::stop_token& stop) {
    std::vector<std::string> wasOnline;
    std::chrono::steady_clock::time_point fetched{};
    // A peer that did not answer is asked again sooner, each time later, up to `refetchEvery`.
    std::chrono::seconds again = refetchEvery;
    std::chrono::seconds backoff = lookEvery;
    bool first = true;
    while (!stop.stop_requested()) {
        std::vector<Notice> notices;
        bool woken = false;
        {
            std::unique_lock lock{mutex_};
            if (!first) {
                wake_.wait_for(lock, stop, lookEvery, [this] {
                    return woken_;
                });
            }
            woken = std::exchange(woken_, false);
            notices.swap(notices_);
        }
        first = false;
        for (Notice& notice : notices) {
            tell(notice);
            secrets::wipeBytes(notice.secret.data(), notice.secret.size());
        }
        if (stop.stop_requested()) {
            return;
        }
        std::vector<Sighting> online = sightings();
        std::erase_if(online, [this](const Sighting& sighting) {
            return !book_.find(sighting.id);
        });
        std::vector<std::string> ids;
        ids.reserve(online.size());
        for (const Sighting& sighting : online) {
            ids.push_back(sighting.id + "@" + sighting.address + ":" +
                          std::to_string(sighting.port));
        }
        std::ranges::sort(ids);
        const auto now = std::chrono::steady_clock::now();
        if (!woken && ids == wasOnline && now - fetched < again) {
            continue;
        }
        wasOnline = std::move(ids);
        fetched = now;
        bool complete = true;
        if (remote_->replace(fetch(online, complete))) {
            changed_();
        }
        if (complete) {
            again = refetchEvery;
            backoff = lookEvery;
        } else {
            again = backoff;
            backoff = std::min(backoff * 2, std::chrono::seconds{refetchEvery});
        }
    }
}

std::vector<library::Game> LanNode::fetch(const std::vector<Sighting>& online, bool& complete) {
    std::error_code ec;
    for (const fs::directory_entry& folder : fs::directory_iterator{coverCache_, ec}) {
        if (!book_.find(folder.path().filename().string())) {
            fs::remove_all(folder.path(), ec);
        }
    }
    std::vector<library::Game> games;
    for (const Sighting& sighting : online) {
        std::optional<crypto::Bytes> secret = book_.secretOf(sighting.id);
        if (!secret) {
            continue;
        }
        const Peer host{sighting.id, sighting.name};
        const PeerClient client{web_, Endpoint{sighting.address, sighting.port}};
        std::string error;
        const std::optional<net::WebClient::Answer> answer =
            client.call("GET", "/lan/library", {}, PeerClient::Signer{book_.id(), *secret},
                        largestListing, error);
        std::optional<std::vector<wire::ListedGame>> listing;
        if (answer && answer->status == ok) {
            listing = wire::readListing(answer->body);
        }
        if (!listing) {
            lucent::warn("lan", "the games of {} cannot be listed: {}", host.name,
                         answer ? "HTTP " + std::to_string(answer->status) : error);
            secrets::wipeBytes(secret->data(), secret->size());
            complete = false;
            continue;
        }
        std::set<fs::path> covers;
        for (const wire::ListedGame& listed : *listing) {
            fs::path cover;
            if (!listed.cover.empty()) {
                cover = coverOf(host, client, *secret, listed.cover);
                covers.insert(cover);
            }
            games.push_back(remoteGame(host, listed, cover));
        }
        secrets::wipeBytes(secret->data(), secret->size());
        lucent::info("lan", "{} lists {} games", host.name, listing->size());
        for (const fs::directory_entry& kept : fs::directory_iterator{coverCache_ / host.id, ec}) {
            if (!covers.contains(kept.path())) {
                fs::remove(kept.path(), ec);
            }
        }
    }
    return games;
}

fs::path LanNode::coverOf(const Peer& host, const PeerClient& client, const crypto::Bytes& secret,
                          const std::string& token) const {
    const fs::path folder = coverCache_ / host.id;
    for (const std::string_view extension : coverExtensions) {
        fs::path cached = folder / (token + std::string{extension});
        std::error_code ec;
        if (fs::is_regular_file(cached, ec)) {
            return cached;
        }
    }
    std::string error;
    const std::optional<net::WebClient::Answer> answer =
        client.call("GET", "/lan/cover/" + token, {}, PeerClient::Signer{book_.id(), secret},
                    largestCover, error);
    if (!answer || answer->status != ok) {
        lucent::warn("lan", "a cover of {} cannot be had: {}", host.name,
                     answer ? "HTTP " + std::to_string(answer->status) : error);
        return {};
    }
    fs::path file = folder / (token + std::string{extensionOf(answer->header("Content-Type"))});
    if (!fileio::writeWhole(file, answer->body, error)) {
        lucent::warn("lan", "a cover of {} cannot be kept: {}", host.name, error);
        return {};
    }
    return file;
}

void LanNode::tell(const Notice& notice) const {
    const PeerClient client{web_, notice.endpoint};
    std::string error;
    const std::optional<net::WebClient::Answer> answer =
        client.call("POST", "/lan/unpair", "{}", PeerClient::Signer{book_.id(), notice.secret},
                    largestNotice, error);
    if (!answer || answer->status != ok) {
        lucent::info("lan", "the unpaired openSU at {} was not told: {}", notice.endpoint.address,
                     answer ? "HTTP " + std::to_string(answer->status) : error);
    }
}

} // namespace opensu::lan
