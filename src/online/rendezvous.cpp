#include "rendezvous.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "secrets/secret.hpp"
#include "stun.hpp"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;
using secrets::crypto::Bytes;

constexpr std::uint8_t kindHello = 0xE1;
constexpr std::uint8_t kindWelcome = 0xE2;
constexpr std::uint8_t kindKnock = 0xE3;
constexpr std::size_t nonceAt = 1;
constexpr std::size_t sealedAt = nonceAt + secrets::crypto::sealNonceBytes;
constexpr std::string_view helloLabel = "opensu-online\nhello";
/// How often each side sends to the other's addresses while they look for each other.
constexpr auto sendEvery = 100ms;
/// Past a home router and a carrier's NAT, and short of most far ends.
constexpr int nearHops = 3;
/// How many addresses and unproved friends are kept, whatever comes.
constexpr std::size_t mostTargets = 12;
constexpr std::size_t mostCallers = 4;

std::span<const std::uint8_t> bytesOf(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

Bytes join(std::span<const std::uint8_t> first, std::span<const std::uint8_t> second) {
    Bytes joined{first.begin(), first.end()};
    joined.insert(joined.end(), second.begin(), second.end());
    return joined;
}

} // namespace

Rendezvous::Rendezvous(const stream::UdpSocket& socket, Role role,
                       std::span<const std::uint8_t> secret)
    : socket_{socket}, role_{role}, secret_{secret.begin(), secret.end()},
      helloKey_{secrets::crypto::hkdfSha256(secret, bytesOf(helloLabel), helloLabel,
                                            secrets::crypto::keyBytes)},
      buffer_(stream::largestDatagram) {
}

Rendezvous::~Rendezvous() {
    secrets::wipeBytes(secret_.data(), secret_.size());
    secrets::wipeBytes(helloKey_.data(), helloKey_.size());
}

SessionKeys Rendezvous::keysWith(std::span<const std::uint8_t> shared,
                                 std::span<const std::uint8_t> hostKey,
                                 std::span<const std::uint8_t> friendKey) const {
    Bytes material = join(shared, secret_);
    const Bytes salt = join(hostKey, friendKey);
    const auto derive = [&material, &salt](std::string_view label) {
        return secrets::crypto::hkdfSha256(material, salt, label, secrets::crypto::keyBytes);
    };
    SessionKeys keys{.linkHost = derive("opensu-online\nlink\nhost"),
                     .linkFriend = derive("opensu-online\nlink\nfriend"),
                     .gameHost = derive("opensu-online\ngame\nhost"),
                     .gameFriend = derive("opensu-online\ngame\nfriend")};
    secrets::wipeBytes(material.data(), material.size());
    return keys;
}

std::string Rendezvous::sealed(std::uint8_t kind, std::string_view plain) const {
    const Bytes nonce = secrets::crypto::randomBytes(secrets::crypto::sealNonceBytes);
    const std::string header(1, static_cast<char>(kind));
    return header + std::string{secrets::crypto::asText(nonce)} +
           secrets::crypto::seal(helloKey_, nonce, header, plain);
}

std::optional<std::string> Rendezvous::opened(std::uint8_t kind, std::string_view datagram) const {
    if (datagram.size() < sealedAt + secrets::crypto::sealTagBytes) {
        return std::nullopt;
    }
    return secrets::crypto::open(helloKey_, bytesOf(datagram.substr(nonceAt, sealedAt - nonceAt)),
                                 std::string(1, static_cast<char>(kind)),
                                 datagram.substr(sealedAt));
}

void Rendezvous::aim(const std::vector<stream::UdpAddress>& addresses) {
    for (const stream::UdpAddress& address : addresses) {
        if (targets_.size() < mostTargets &&
            std::ranges::find(targets_, address) == targets_.end()) {
            targets_.push_back(address);
        }
    }
    nextSend_ = {};
}

void Rendezvous::sendDue() {
    const Clock::time_point now = Clock::now();
    if (now < nextSend_) {
        return;
    }
    nextSend_ = now + sendEvery;
    const std::string message = role_ == Role::Friend
                                    ? sealed(kindHello, secrets::crypto::asText(own_.publicKey()))
                                    : sealed(kindKnock, {});
    for (const stream::UdpAddress& target : targets_) {
        if (near_) {
            socket_.sendWithin(target, message, nearHops);
        } else {
            socket_.send(target, message);
        }
    }
}

void Rendezvous::keepNear() {
    near_ = true;
}

void Rendezvous::step(std::chrono::milliseconds wait) {
    if (found_) {
        return;
    }
    sendDue();
    stream::UdpAddress from;
    std::optional<std::size_t> length = socket_.receive(buffer_, from, wait);
    while (length && !found_) {
        take({buffer_.data(), *length}, from);
        length = socket_.receive(buffer_, from, 0ms);
    }
}

std::string Rendezvous::takeStun() {
    return std::exchange(stun_, {});
}

void Rendezvous::take(std::string_view datagram, const stream::UdpAddress& from) {
    if (datagram.empty()) {
        ++dropped_;
        return;
    }
    if (isStun(datagram)) {
        stun_ = datagram;
        return;
    }
    const auto kind = static_cast<std::uint8_t>(datagram.front());
    if (kind == kindHello && role_ == Role::Host) {
        takeHello(datagram, from);
    } else if (kind == kindWelcome && role_ == Role::Friend) {
        takeWelcome(datagram, from);
    } else if (kind == kindKnock && role_ == Role::Friend) {
        takeKnock(datagram, from);
    } else if (role_ == Role::Host) {
        takeSealed(datagram, from);
    } else {
        ++dropped_;
    }
}

void Rendezvous::takeHello(std::string_view datagram, const stream::UdpAddress& from) {
    const std::optional<std::string> plain = opened(kindHello, datagram);
    if (!plain || plain->size() != secrets::crypto::keyBytes) {
        ++dropped_;
        return;
    }
    const std::span<const std::uint8_t> friendKey = bytesOf(*plain);
    const Bytes key{friendKey.begin(), friendKey.end()};
    if (!callers_.contains(key)) {
        const std::optional<Bytes> shared = own_.agree(friendKey);
        if (!shared || callers_.size() >= mostCallers) {
            ++dropped_;
            return;
        }
        SessionKeys keys = keysWith(*shared, own_.publicKey(), friendKey);
        auto opener = std::make_unique<stream::PacketOpener>(keys.linkFriend);
        callers_.emplace(key, Caller{.keys = std::move(keys), .opener = std::move(opener)});
    }
    const std::string answer = std::string{secrets::crypto::asText(own_.publicKey())} + *plain;
    socket_.send(from, sealed(kindWelcome, answer));
}

void Rendezvous::takeWelcome(std::string_view datagram, const stream::UdpAddress& from) {
    const std::optional<std::string> plain = opened(kindWelcome, datagram);
    if (!plain || plain->size() != 2 * secrets::crypto::keyBytes) {
        ++dropped_;
        return;
    }
    const std::span<const std::uint8_t> keys = bytesOf(*plain);
    const std::span<const std::uint8_t> hostKey = keys.first(secrets::crypto::keyBytes);
    if (!secrets::crypto::sameBytes(keys.last(secrets::crypto::keyBytes), own_.publicKey())) {
        ++dropped_;
        return;
    }
    const std::optional<Bytes> shared = own_.agree(hostKey);
    if (!shared) {
        ++dropped_;
        return;
    }
    found_ = Found{.peer = from, .keys = keysWith(*shared, hostKey, own_.publicKey())};
}

void Rendezvous::takeKnock(std::string_view datagram, const stream::UdpAddress& from) {
    if (!opened(kindKnock, datagram)) {
        ++dropped_;
        return;
    }
    aim({from});
    near_ = false;
    nextSend_ = {};
}

void Rendezvous::takeSealed(std::string_view datagram, const stream::UdpAddress& from) {
    for (auto& [key, caller] : callers_) {
        if (caller.opener->open(datagram)) {
            found_ = Found{.peer = from, .keys = std::move(caller.keys)};
            callers_.clear();
            return;
        }
    }
    ++dropped_;
}

} // namespace opensu::online
