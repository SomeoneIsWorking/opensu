// Two machines over loopback through the shipping rendezvous, with what an attacker on the internet
// can do thrown at the host's port: forged datagrams, a wrong code, and datagrams heard and
// replayed.
#include "online/rendezvous.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "secrets/crypto.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using namespace opensu::online;
using opensu::secrets::crypto::Bytes;
using opensu::secrets::crypto::randomBytes;
using opensu::stream::UdpAddress;
using opensu::stream::UdpSocket;
using opensu::test::expect;
using opensu::test::need;

UdpAddress loopback(std::uint16_t port) {
    return need(opensu::stream::udpAddress("127.0.0.1", port), "loopback");
}

void run(Rendezvous& a, Rendezvous& b, int rounds = 40) {
    for (int at = 0; at < rounds && !(a.found() && b.found()); ++at) {
        a.step(5ms);
        b.step(5ms);
    }
}

/// Reads whatever `socket` has, up to `wait`.
std::vector<std::string> heard(const UdpSocket& socket, std::chrono::milliseconds wait) {
    std::vector<std::string> datagrams;
    std::vector<char> buffer(opensu::stream::largestDatagram);
    UdpAddress from;
    auto length = socket.receive(buffer, from, wait);
    while (length) {
        datagrams.emplace_back(buffer.data(), *length);
        length = socket.receive(buffer, from, wait);
    }
    return datagrams;
}

/// The friend's first sealed link datagram, which proves it to the host.
std::string proof(const Found& found) {
    opensu::stream::PacketSeal seal{found.keys.linkFriend};
    return seal.seal(opensu::stream::PacketKind::Netplay, "ping");
}

void bothFindEachOther() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket friendSocket{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    Rendezvous joiner{friendSocket, Rendezvous::Role::Friend, secret};
    joiner.aim({loopback(hostSocket.port())});
    run(host, joiner);
    expect(joiner.found().has_value(), "the friend is welcomed");
    expect(!host.found().has_value(), "the host waits for proof");
    friendSocket.send(loopback(hostSocket.port()),
                      proof(need(joiner.found(), "the friend found the host")));
    run(host, joiner);
    expect(host.found().has_value(), "a datagram under the session keys proves the friend");
    expect(need(host.found(), "found").peer == loopback(friendSocket.port()),
           "the host has the friend's address");
    expect(need(host.found(), "found").keys == need(joiner.found(), "found").keys,
           "both hold the same keys");
    expect(
        need(host.found(), "found").keys.gameHost != need(host.found(), "found").keys.gameFriend &&
            need(host.found(), "found").keys.linkHost != need(host.found(), "found").keys.gameHost,
        "no key serves two senders or two purposes");
}

void knockReachesAFriendTheHostCanOnlyDial() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket friendSocket{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    Rendezvous joiner{friendSocket, Rendezvous::Role::Friend, secret};
    host.aim({loopback(friendSocket.port())});
    run(host, joiner);
    expect(joiner.found().has_value(), "a knock tells the friend where to say hello");
}

void aFriendKeptNearAnswersAKnockInFull() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket friendSocket{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    Rendezvous joiner{friendSocket, Rendezvous::Role::Friend, secret};
    joiner.aim({loopback(hostSocket.port())});
    joiner.keepNear();
    host.aim({loopback(friendSocket.port())});
    run(host, joiner);
    expect(joiner.found().has_value(), "a friend kept near still answers the host");
}

void forgeriesAreDropped() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket attacker{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    const UdpAddress target = loopback(hostSocket.port());
    attacker.send(target, std::string(61, 'x'));
    std::string looksLikeHello(61, '\0');
    looksLikeHello[0] = static_cast<char>(0xE1);
    attacker.send(target, looksLikeHello);
    std::string looksLikePacket(64, '\0');
    looksLikePacket[0] = 3;
    looksLikePacket[1] = 13;
    attacker.send(target, looksLikePacket);
    attacker.send(target, "");
    for (int at = 0; at < 5; ++at) {
        host.step(5ms);
    }
    expect(host.dropped() == 4, "each forgery is dropped");
    expect(heard(attacker, 20ms).empty(), "and none gets an answer");
    expect(!host.found().has_value(), "and none is a friend");
}

void wrongCodeGetsNowhere() {
    const UdpSocket hostSocket{0};
    const UdpSocket friendSocket{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, randomBytes(secretBytes)};
    Rendezvous joiner{friendSocket, Rendezvous::Role::Friend, randomBytes(secretBytes)};
    joiner.aim({loopback(hostSocket.port())});
    host.aim({loopback(friendSocket.port())});
    run(host, joiner, 30);
    expect(!host.found() && !joiner.found(), "a wrong code finds nobody");
    expect(host.dropped() > 0 && joiner.dropped() > 0, "and both drop what the other sends");
}

void replaysGetNowhere() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket tap{0};
    const UdpSocket friendSocket{0};
    const UdpSocket attacker{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    Rendezvous joiner{friendSocket, Rendezvous::Role::Friend, secret};
    joiner.aim({loopback(tap.port())});
    joiner.step(1ms);
    const std::vector<std::string> hellos = heard(tap, 20ms);
    expect(!hellos.empty(), "the tap heard a hello");

    // A heard hello gets its welcome back, but the welcome is no use without the friend's key.
    attacker.send(loopback(hostSocket.port()), hellos.front());
    host.step(5ms);
    const std::vector<std::string> welcomes = heard(attacker, 20ms);
    expect(welcomes.size() == 1, "a replayed hello is answered like any other");
    expect(!host.found().has_value(), "and proves nothing");

    // The real friend finishes and the code is spent.
    friendSocket.send(loopback(hostSocket.port()), "");
    joiner.aim({loopback(hostSocket.port())});
    run(host, joiner);
    expect(joiner.found().has_value(), "the friend still gets in");
    const std::string sealed = proof(need(joiner.found(), "the friend found the host"));
    attacker.send(loopback(hostSocket.port()), sealed);
    run(host, joiner);
    expect(host.found() && need(host.found(), "found").peer == loopback(attacker.port()),
           "the first to prove with the friend's own datagram wins the address");
    const std::uint64_t dropped = host.dropped();
    attacker.send(loopback(hostSocket.port()), sealed);
    host.step(5ms);
    expect(host.dropped() == dropped, "once found, the rendezvous takes nothing more");
}

void aReplayedProofIsOpenedOnce() {
    const Bytes secret = randomBytes(secretBytes);
    const UdpSocket hostSocket{0};
    const UdpSocket friendSocket{0};
    const UdpSocket attacker{0};
    Rendezvous host{hostSocket, Rendezvous::Role::Host, secret};
    Rendezvous first{friendSocket, Rendezvous::Role::Friend, secret};
    first.aim({loopback(hostSocket.port())});
    run(host, first);
    expect(first.found().has_value(), "welcomed");
    opensu::stream::PacketSeal seal{need(first.found(), "found").keys.linkHost};
    attacker.send(loopback(hostSocket.port()),
                  seal.seal(opensu::stream::PacketKind::Netplay, "wrong sender's key"));
    host.step(5ms);
    expect(!host.found().has_value(), "a datagram under the host's own key proves no friend");
}

} // namespace

int main() try {
    bothFindEachOther();
    knockReachesAFriendTheHostCanOnlyDial();
    aFriendKeptNearAnswersAKnockInFull();
    forgeriesAreDropped();
    wrongCodeGetsNowhere();
    replaysGetNowhere();
    aReplayedProofIsOpenedOnce();
    std::printf("rendezvous: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
