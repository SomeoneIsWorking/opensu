// The shells' link over loopback through its shipping code: messages arrive whole and in order over
// a lossy path, a replayed datagram is not taken twice, and what is oversized or unknown breaks the
// link before anything of that size is held.
#include "online/reliable_link.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "fileio/little_endian.hpp"
#include "secrets/crypto.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using namespace opensu::online;
using opensu::fileio::appendLittleEndian;
using opensu::secrets::crypto::Bytes;
using opensu::secrets::crypto::randomBytes;
using opensu::stream::PacketKind;
using opensu::stream::PacketSeal;
using opensu::stream::UdpAddress;
using opensu::stream::UdpSocket;
using opensu::test::expect;
using opensu::test::need;

UdpAddress loopback(std::uint16_t port) {
    return need(opensu::stream::udpAddress("127.0.0.1", port), "loopback");
}

struct Ends {
    Bytes keyA{randomBytes(opensu::secrets::crypto::keyBytes)};
    Bytes keyB{randomBytes(opensu::secrets::crypto::keyBytes)};
    UdpSocket socketA{0};
    UdpSocket socketB{0};
    ReliableLink a{socketA, loopback(socketB.port()), keyA, keyB};
    ReliableLink b{socketB, loopback(socketA.port()), keyB, keyA};

    void pump(int rounds) {
        for (int at = 0; at < rounds; ++at) {
            a.pump(1ms);
            b.pump(1ms);
        }
    }
};

/// A peer that holds the key and sends what no openSU does.
struct Rogue {
    Bytes key{randomBytes(opensu::secrets::crypto::keyBytes)};
    Bytes own{randomBytes(opensu::secrets::crypto::keyBytes)};
    UdpSocket socket{0};
    UdpSocket target{0};
    PacketSeal seal{key};
    ReliableLink link{target, loopback(socket.port()), own, key};

    void send(std::uint32_t sequence, std::string_view chunk) {
        std::string payload(1, '\1');
        appendLittleEndian(payload, sequence, 4);
        payload.append(chunk);
        socket.send(loopback(target.port()), seal.seal(PacketKind::Netplay, payload));
    }
};

std::string header(std::uint8_t kind, std::uint32_t size) {
    std::string bytes(1, static_cast<char>(kind));
    appendLittleEndian(bytes, size, 4);
    return bytes;
}

void messagesArriveInOrder() {
    Ends ends;
    std::string big(std::size_t{300} * 1024, 'x');
    for (std::size_t at = 0; at < big.size(); at += 97) {
        big[at] = static_cast<char>(at);
    }
    ends.a.send(LinkKind::Ask, "ask");
    ends.a.send(LinkKind::Save, big);
    ends.a.send(LinkKind::Go, "go");
    std::vector<LinkMessage> got;
    for (int at = 0; at < 4000 && got.size() < 3; ++at) {
        ends.pump(1);
        while (auto message = ends.b.take()) {
            got.push_back(std::move(*message));
        }
    }
    expect(got.size() == 3, "all three arrive");
    expect(got[0].kind == LinkKind::Ask && got[0].body == "ask", "the ask first");
    expect(got[1].kind == LinkKind::Save && got[1].body == big, "the save whole");
    expect(got[2].kind == LinkKind::Go, "go last");
    ends.pump(30);
    expect(ends.a.flushed(), "the sender knows it all arrived");
    expect(ends.a.roundTrip().has_value(), "the round trip is measured");
}

void oversizedBreaksTheLink() {
    Rogue rogue;
    rogue.send(0, header(static_cast<std::uint8_t>(LinkKind::Ask), 64 * 1024 + 1));
    rogue.link.pump(50ms);
    expect(rogue.link.broken(), "an ask over its cap breaks the link at once");
    expect(!rogue.link.take(), "and nothing of it is handed on");

    Rogue saves;
    saves.send(0, header(static_cast<std::uint8_t>(LinkKind::Save), 0xFFFFFFFFU));
    saves.link.pump(50ms);
    expect(saves.link.broken(), "a save of 4 GiB is refused from its header");
}

void unknownKindBreaksTheLink() {
    Rogue rogue;
    rogue.send(0, header(0x7F, 4));
    rogue.link.pump(50ms);
    expect(rogue.link.broken(), "a message of no known kind breaks the link");
}

void anOverlongChunkBreaksTheLink() {
    Rogue rogue;
    rogue.send(0, std::string(ReliableLink::chunkBytes + 1, '\1'));
    rogue.link.pump(50ms);
    expect(rogue.link.broken(), "a chunk over the size is refused");
}

void replaysAndForgeriesAreIgnored() {
    Rogue rogue;
    PacketSeal copy{rogue.key};
    std::string payload(1, '\1');
    appendLittleEndian(payload, 0, 4);
    payload += header(static_cast<std::uint8_t>(LinkKind::Ask), 2) + "hi";
    const std::string datagram = copy.seal(PacketKind::Netplay, payload);
    const UdpAddress target = loopback(rogue.target.port());
    for (int times = 0; times < 3; ++times) {
        rogue.socket.send(target, datagram);
    }
    rogue.socket.send(target, std::string(100, 'x'));
    rogue.link.pump(50ms);
    const auto first = rogue.link.take();
    expect(first && first->body == "hi", "the first copy is taken");
    expect(!rogue.link.take(), "the replays are not");
    expect(!rogue.link.broken(), "and a forgery does nothing");
}

void aFarSequenceIsNotHeld() {
    Rogue rogue;
    rogue.send(ReliableLink::window + 10, "too far ahead");
    rogue.send(0xFFFFFFFFU, "and further");
    rogue.link.pump(50ms);
    expect(!rogue.link.take() && !rogue.link.broken(), "what is past the window is ignored");
}

} // namespace

int main() try {
    messagesArriveInOrder();
    oversizedBreaksTheLink();
    unknownKindBreaksTheLink();
    anOverlongChunkBreaksTheLink();
    replaysAndForgeriesAreIgnored();
    aFarSequenceIsNotHeld();
    std::printf("reliable link: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
