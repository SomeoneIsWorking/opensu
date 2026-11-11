// stream: remote play's sealed datagrams, their payloads, and a host and guest session over
// loopback UDP.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "lan/pairing_keys.hpp"
#include "stream/audio_codec.hpp"
#include "stream/frame_parity.hpp"
#include "stream/guest_audio.hpp"
#include "stream/guest_stream.hpp"
#include "stream/host_stream.hpp"
#include "stream/host_video.hpp"
#include "stream/link_cap.hpp"
#include "stream/link_rate.hpp"
#include "stream/packet_seal.hpp"
#include "stream/predicted_frames.hpp"
#include "stream/remote_reach.hpp"
#include "stream/stream_wire.hpp"
#include "stream/token_bucket.hpp"
#include "stream/udp_socket.hpp"
#include "stream/video_codec.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu;
using stream::PacketKind;

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

bool waitFor(const std::function<bool()>& holds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (std::chrono::steady_clock::now() < until) {
        if (holds()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return holds();
}

lan::StreamKeys keysFor(std::uint8_t fill) {
    const std::vector<std::uint8_t> secret(32, fill);
    const std::vector<std::uint8_t> guestSalt(16, 1);
    const std::vector<std::uint8_t> hostSalt(16, 2);
    return lan::streamKeys(secret, guestSalt, hostSalt);
}

void datagramsOpenOnce() {
    const lan::StreamKeys keys = keysFor(7);
    stream::PacketSeal seal{keys.guest};
    stream::PacketOpener opener{keys.guest};
    stream::PacketOpener stranger{keys.host};

    const std::string first = seal.seal(PacketKind::Pad, "pad");
    expect(first.size() == 3 + stream::sealOverhead, "a datagram is its payload and the overhead");
    expect(!stranger.open(first), "the other way's key does not open it");
    const std::optional<stream::Opened> opened = opener.open(first);
    expect(opened && opened->kind == PacketKind::Pad && opened->payload == "pad" &&
               opened->counter == 0,
           "the session's key opens it");
    expect(!opener.open(first), "and only once");

    std::string changed = seal.seal(PacketKind::Pad, "pad");
    changed.back() = static_cast<char>(changed.back() ^ 1);
    expect(!opener.open(changed), "a changed datagram does not open");
    std::string relabelled = seal.seal(PacketKind::Pad, "pad");
    relabelled[1] = static_cast<char>(PacketKind::End);
    expect(!opener.open(relabelled), "nor one whose kind was changed");

    const std::string late = seal.seal(PacketKind::Video, "a");
    const std::string early = seal.seal(PacketKind::Video, "b");
    expect(opener.open(early) && opener.open(late), "reordered ones inside the window open");
    std::string lost;
    for (std::size_t i = 0; i <= stream::PacketOpener::window; ++i) {
        const std::string sealed = seal.seal(PacketKind::Video, "c");
        if (i == 0) {
            lost = sealed;
        } else {
            expect(opener.open(sealed).has_value(), "every new one opens");
        }
    }
    expect(!opener.open(lost), "one older than the window does not");
}

void payloadsReadAsWritten() {
    namespace wire = stream::wire;
    const wire::VideoPacket video{.frame = 9,
                                  .width = 1280,
                                  .height = 720,
                                  .codec = 2,
                                  .key = true,
                                  .part = 4,
                                  .parts = 7,
                                  .capturedUs = 1,
                                  .encodedUs = 2,
                                  .sentUs = 3,
                                  .data = "bits"};
    const std::optional<wire::VideoPacket> readVideo = wire::readVideoPacket(wire::write(video));
    expect(readVideo && readVideo->frame == 9 && readVideo->width == 1280 &&
               readVideo->height == 720 && readVideo->codec == 2 && readVideo->key &&
               readVideo->part == 4 && readVideo->parts == 7 && readVideo->capturedUs == 1 &&
               readVideo->encodedUs == 2 && readVideo->sentUs == 3 && readVideo->data == "bits",
           "a video packet reads as written");
    expect(!wire::readVideoPacket(wire::write(video).substr(0, wire::videoHeaderBytes)),
           "one with no data does not");
    wire::VideoPacket pastItsFrame = video;
    pastItsFrame.part = 7;
    expect(!wire::readVideoPacket(wire::write(pastItsFrame)), "nor a part past its frame's last");

    const std::string probe =
        wire::write(wire::Probe{.burst = 3, .index = 41, .sentUs = 1ULL << 40U}, 1428);
    const std::optional<wire::Probe> readProbe = wire::readProbe(probe);
    expect(probe.size() == 1428 && readProbe && readProbe->burst == 3 && readProbe->index == 41 &&
               readProbe->sentUs == 1ULL << 40U,
           "a probe reads as written, padded to its size");
    const wire::Report report{.decoders = 5,
                              .datagrams = 1U << 20U,
                              .bytes = 1ULL << 40U,
                              .newest = 77,
                              .probeBurst = 2,
                              .probeDatagrams = 900,
                              .probeQueueUs = 4100,
                              .pictureWidth = 1920,
                              .pictureHeight = 1262,
                              .framesPerSecond = 30};
    expect(wire::readReport(wire::write(report)) == report, "a report reads as written");

    using gamepad::Button;
    const wire::PadMapping mapping{
        .map = gamepad::ForwardMap{.buttons = {{Button::A, {Button::B}},
                                               {Button::X, {}},
                                               {Button::L2, {Button::R1, Button::R2}}},
                                   .chords = {{gamepad::Chord{.modifier = Button::Select,
                                                              .trigger = Button::Start},
                                               {Button::L3}}}},
        .holdDelay = std::chrono::milliseconds{500}};
    expect(wire::readPadMapping(wire::write(mapping)) == mapping,
           "a mapping reads as written: swapped, unsent and chorded buttons");
    expect(!wire::readPadMapping(wire::write(mapping) + "x"), "a longer one does not");

    const wire::PadState pad{.buttons = 0x81,
                             .leftX = -32768,
                             .leftY = 32767,
                             .rightX = -1,
                             .rightY = 1,
                             .leftTrigger = 255,
                             .rightTrigger = 3};
    expect(wire::readPadState(wire::write(pad)) == pad, "a pad reads as written");
    expect(!wire::readPadState(wire::write(pad) + "x"), "a longer pad does not");

    const wire::KeyState keys{{4, 5, 225}};
    expect(wire::readKeyState(wire::write(keys)) == keys, "held keys read as written");
    std::string tooMany(1, static_cast<char>(wire::heldKeys + 1));
    tooMany.append(wire::heldKeys + 1, '\x04');
    expect(!wire::readKeyState(tooMany), "more than a keyboard rolls over does not");

    const wire::MouseState mouse{.x = 65535, .y = 2, .buttons = 5, .wheel = -3};
    expect(wire::readMouseState(wire::write(mouse)) == mouse, "a pointer reads as written");

    const std::optional<wire::End> end = wire::readEnd("over\nand out");
    expect(end && end->reason == "over and out", "an end's reason keeps no control characters");
}

void aSessionCarriesInputAndEnds() {
    std::mutex mutex;
    std::optional<stream::wire::PadState> heard;
    std::vector<std::string> pictures;
    std::optional<stream::wire::PadMapping> mapped;
    const stream::VideoCodecs pyroWave = stream::codecBit(stream::VideoCodec::PyroWave);
    stream::HostStream host{
        stream::HostStream::Setup{.keys = keysFor(3),
                                  .guest = "Desk",
                                  .pad =
                                      [&](const stream::wire::PadState& pad) {
                                          const std::lock_guard lock{mutex};
                                          heard = pad;
                                      },
                                  .keyboard = {},
                                  .mouse = {},
                                  .mapping =
                                      [&](const stream::wire::PadMapping& mapping) {
                                          const std::lock_guard lock{mutex};
                                          mapped = mapping;
                                      },
                                  .encoders = pyroWave}};
    const std::optional<stream::UdpAddress> address = stream::udpAddress("127.0.0.1", host.port());
    if (!address || host.port() == 0) {
        fail("the host takes packets on a port of its own");
    }

    // Someone else's session is not heard; the host waits for its own guest.
    stream::GuestStream stranger{stream::GuestStream::Setup{
        .host = *address, .keys = keysFor(4), .video = {}, .audio = {}, .decoders = pyroWave}};
    stranger.pad(stream::wire::PadState{.buttons = 2});
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    expect(!host.joined(), "a datagram sealed with other keys is no guest");

    stream::GuestStream guest{stream::GuestStream::Setup{
        .host = *address,
        .keys = keysFor(3),
        .video =
            [&](const stream::wire::VideoPacket& packet, std::uint64_t /*receivedUs*/,
                std::int64_t /*hostAheadUs*/) {
                const std::lock_guard lock{mutex};
                pictures.push_back(packet.data);
                return false;
            },
        .audio = {},
        .decoders = pyroWave}};
    const stream::wire::PadMapping swapped{
        .map = gamepad::ForwardMap{.buttons = {{gamepad::Button::A, {gamepad::Button::B}}},
                                   .chords = {}},
        .holdDelay = std::chrono::milliseconds{500}};
    guest.mapping(swapped);
    guest.pad(stream::wire::PadState{.buttons = 1, .leftX = 100});
    expect(waitFor([&] {
               const std::lock_guard lock{mutex};
               return heard && heard->buttons == 1 && heard->leftX == 100;
           }),
           "the guest's pad reaches the host");
    expect(host.joined(), "and the host knows its guest");
    expect(waitFor([&guest] {
               return guest.timing().known;
           }),
           "the guest's pings are answered");
    expect(guest.timing().roundTripUs < 1000000, "over loopback in well under a second");
    expect(waitFor([&] {
               const std::lock_guard lock{mutex};
               return mapped == swapped;
           }),
           "the guest's mapping reaches the host");
    expect(waitFor([&host] {
               return host.plan().has_value();
           }) &&
               host.plan().value_or(stream::LinkPlan{.codec = stream::VideoCodec::H264}).codec ==
                   stream::VideoCodec::PyroWave,
           "the host probes the link and codes with PyroWave over loopback");
    std::printf("stream: loopback %s\n", host.linkSummary().c_str());

    host.send(PacketKind::Video,
              stream::wire::write(stream::wire::VideoPacket{
                  .frame = 1, .width = 2, .height = 2, .parts = 1, .data = "frame"}));
    expect(waitFor([&] {
               const std::lock_guard lock{mutex};
               return pictures.size() == 1 && pictures.front() == "frame";
           }),
           "what the host sends reaches the guest");
    expect(!stranger.over() || stranger.over()->find("stopped answering") != std::string::npos,
           "the stranger is never answered");

    guest.end("the player ended remote play");
    expect(waitFor([&host] {
               return host.over().has_value();
           }) &&
               host.over().value_or("").find("the player ended remote play") != std::string::npos,
           "the guest's end ends the host's side, with its reason");
}

void theHostsEndReachesTheGuest() {
    stream::HostStream host{stream::HostStream::Setup{.keys = keysFor(5),
                                                      .guest = "Desk",
                                                      .pad = {},
                                                      .keyboard = {},
                                                      .mouse = {},
                                                      .mapping = {},
                                                      .encoders = 0}};
    const std::optional<stream::UdpAddress> address = stream::udpAddress("127.0.0.1", host.port());
    if (!address) {
        fail("the host takes packets on a port of its own");
    }
    stream::GuestStream guest{stream::GuestStream::Setup{
        .host = *address, .keys = keysFor(5), .video = {}, .audio = {}, .decoders = 0}};
    expect(waitFor([&host] {
               return host.joined();
           }),
           "the guest's first ping joins it");
    host.end("the game ended");
    expect(waitFor([&guest] {
               return guest.over().has_value();
           }) &&
               guest.over().value_or("").find("the game ended") != std::string::npos,
           "the host's end ends the guest's side, with its reason");
}

void noCommonCodecEndsTheSession() {
    stream::HostStream host{
        stream::HostStream::Setup{.keys = keysFor(6),
                                  .guest = "Tablet",
                                  .pad = {},
                                  .keyboard = {},
                                  .mouse = {},
                                  .mapping = {},
                                  .encoders = stream::codecBit(stream::VideoCodec::PyroWave)}};
    const std::optional<stream::UdpAddress> address = stream::udpAddress("127.0.0.1", host.port());
    if (!address) {
        fail("the host takes packets on a port of its own");
    }
    stream::GuestStream guest{
        stream::GuestStream::Setup{.host = *address,
                                   .keys = keysFor(6),
                                   .video = {},
                                   .audio = {},
                                   .decoders = stream::codecBit(stream::VideoCodec::Hevc)}};
    expect(waitFor([&guest] {
               return guest.over().has_value();
           }) &&
               guest.over().value_or("").find("no video format") != std::string::npos,
           "a guest that can decode nothing the host codes is told so");
}

/// A 4:2:0 picture as Gamescope gives it: luma rows, then interleaved chroma, `stride` apart.
struct Nv12 {
    int width{0};
    int height{0};
    std::size_t stride{0};
    std::vector<std::uint8_t> luma;
    std::vector<std::uint8_t> chroma;
};

Nv12 testPicture(int width, int height) {
    Nv12 picture{.width = width,
                 .height = height,
                 .stride = static_cast<std::size_t>(width) + 64,
                 .luma = {},
                 .chroma = {}};
    picture.luma.resize(picture.stride * static_cast<std::size_t>(height));
    picture.chroma.resize(picture.stride * static_cast<std::size_t>(height / 2));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool square = x >= 40 && x < 104 && y >= 30 && y < 94;
            picture
                .luma[static_cast<std::size_t>(y) * picture.stride + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(square ? 235 : (x + y) % 200 + 20);
        }
    }
    for (int y = 0; y < height / 2; ++y) {
        for (int x = 0; x < width / 2; ++x) {
            const std::size_t at =
                static_cast<std::size_t>(y) * picture.stride + 2 * static_cast<std::size_t>(x);
            picture.chroma[at] = static_cast<std::uint8_t>(64 + x % 128);
            picture.chroma[at + 1] = static_cast<std::uint8_t>(192 - y % 128);
        }
    }
    return picture;
}

/// `coded` as the packets the host sends of frame `number`.
std::vector<stream::wire::VideoPacket> packetsOf(const stream::CodedFrame& coded,
                                                 std::uint32_t number, const Nv12& source,
                                                 stream::VideoCodec codec) {
    std::vector<stream::wire::VideoPacket> packets;
    packets.reserve(coded.packets.size());
    for (std::size_t i = 0; i < coded.packets.size(); ++i) {
        packets.push_back(
            stream::wire::VideoPacket{.frame = number,
                                      .width = static_cast<std::uint16_t>(source.width),
                                      .height = static_cast<std::uint16_t>(source.height),
                                      .codec = static_cast<std::uint8_t>(codec),
                                      .key = coded.key,
                                      .part = static_cast<std::uint16_t>(i),
                                      .parts = static_cast<std::uint16_t>(coded.packets.size()),
                                      .data = coded.packets[i]});
    }
    return packets;
}

stream::CapturedFrame captured(const Nv12& source) {
    return stream::CapturedFrame{.width = source.width,
                                 .height = source.height,
                                 .stride = source.stride,
                                 .luma = source.luma,
                                 .chroma = source.chroma,
                                 .capturedUs = 0};
}

/// The mean difference per channel between `decoded` and `source` through the one colour
/// conversion.
double meanError(const Nv12& source, const stream::Picture& decoded) {
    std::vector<std::uint8_t> expected;
    stream::rgbaFromYcbcr(stream::YcbcrPlanes{.luma = source.luma,
                                              .blue = source.chroma,
                                              .red = std::span{source.chroma}.subspan(1),
                                              .width = source.width,
                                              .height = source.height,
                                              .lumaStride = source.stride,
                                              .chromaStride = source.stride,
                                              .chromaStep = 2},
                          expected);
    if (expected.size() != decoded.rgba.size()) {
        return 255.0;
    }
    double error = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        error += std::abs(static_cast<int>(expected[i]) - static_cast<int>(decoded.rgba[i]));
    }
    return error / static_cast<double>(expected.size());
}

/// A PyroWave rate a wired link carries.
constexpr std::uint64_t pyroWaveRate = 150'000'000;

#ifndef __APPLE__
void picturesSurviveTheCodec() {
    const Nv12 source = testPicture(256, 144);
    const std::unique_ptr<stream::VideoEncoder> encoder =
        stream::openEncoder(stream::VideoCodec::PyroWave);
    const stream::CodedFrame coded = encoder->encode(captured(source), pyroWaveRate, false);
    expect(coded.key, "every PyroWave frame stands on its own");
    expect(coded.packets.size() > 1, "a picture is cut into several packets");
    for (const std::string& packet : coded.packets) {
        expect(!packet.empty() && packet.size() <= stream::videoPacketBytes,
               "each fits one datagram");
    }
    const std::vector<stream::wire::VideoPacket> packets =
        packetsOf(coded, 1, source, stream::VideoCodec::PyroWave);

    const std::unique_ptr<stream::VideoDecoder> decoder =
        stream::openDecoder(stream::VideoCodec::PyroWave);
    expect(decoder->intraOnly(), "and so may be skipped");
    for (std::size_t i = 0; i + 1 < packets.size(); ++i) {
        expect(!decoder->push(packets[i]), "a frame is not whole before its last packet");
    }
    expect(decoder->push(packets.back()), "and is once all of them came");
    stream::Picture decoded;
    expect(decoder->decode(decoded, stream::PictureMemory::Cpu), "a whole frame decodes");
    expect(decoded.width == 256 && decoded.height == 144 &&
               decoded.rgba.size() == std::size_t{256} * 144 * 4,
           "at its own size");
    const double error = meanError(source, decoded);
    std::printf("stream: pyrowave mean error %.3f over %zu packets\n", error, packets.size());
    expect(error < 2.0, "the decoded picture is the one coded, to within the codec's loss");

    const std::vector<stream::wire::VideoPacket> next =
        packetsOf(encoder->encode(captured(source), pyroWaveRate, false), 2, source,
                  stream::VideoCodec::PyroWave);
    for (std::size_t i = 1; i < next.size(); ++i) {
        expect(!decoder->push(next[i]), "a frame missing a packet is never whole");
    }
    bool whole = false;
    for (const stream::wire::VideoPacket& packet :
         packetsOf(encoder->encode(captured(source), pyroWaveRate, false), 3, source,
                   stream::VideoCodec::PyroWave)) {
        whole = decoder->push(packet);
    }
    expect(whole, "and the next frame drops it and decodes");
}
#endif

/// Each hardware codec this machine both codes and decodes, over pictures that move: only key
/// frames stand alone, and after a lost packet nothing decodes until the next key frame.
/// `stream_test --write-coded-picture FILE` remakes `coded_picture.bin` on a system that encodes:
/// each packet of one frame as written to the wire, after its length in 4 little-endian bytes.
void writeCodedPicture(const char* path) {
    const Nv12 source = testPicture(256, 144);
    const stream::CodedFrame coded = stream::openEncoder(stream::VideoCodec::PyroWave)
                                         ->encode(captured(source), pyroWaveRate, false);
    std::ofstream file{path, std::ios::binary};
    for (const stream::wire::VideoPacket& packet :
         packetsOf(coded, 1, source, stream::VideoCodec::PyroWave)) {
        const std::string bytes = stream::wire::write(packet);
        for (int shift = 0; shift < 32; shift += 8) {
            file.put(static_cast<char>((bytes.size() >> shift) & 0xffU));
        }
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    expect(file.good(), "the coded picture is written");
}

/// macOS has no PyroWave encoder, so its decoder is checked on a frame coded on Linux.
void aPictureCodedElsewhereDecodes() {
    std::ifstream file{OPENSU_CODED_PICTURE, std::ios::binary};
    const std::string all{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    expect(!all.empty(), "the coded picture is there");
    const std::unique_ptr<stream::VideoDecoder> decoder =
        stream::openDecoder(stream::VideoCodec::PyroWave);
    bool whole = false;
    for (std::size_t at = 0; at + 4 <= all.size();) {
        std::size_t length = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            length |= static_cast<std::size_t>(static_cast<unsigned char>(all[at + i])) << (8 * i);
        }
        at += 4;
        const std::optional<stream::wire::VideoPacket> packet =
            stream::wire::readVideoPacket(std::string_view{all}.substr(at, length));
        if (!packet) {
            fail("each packet reads back");
        }
        whole = decoder->push(*packet);
        at += length;
    }
    expect(whole, "its last packet makes the frame whole");
    stream::Picture decoded;
    expect(decoder->decode(decoded, stream::PictureMemory::Cpu), "the frame decodes");
    const double error = meanError(testPicture(256, 144), decoded);
    std::printf("stream: coded elsewhere, mean error %.3f\n", error);
    expect(error < 2.0, "to the picture that was coded");
}

void picturesSurviveTheHardwareCodecs() {
    const stream::VideoCodecs both = stream::encodableHere() & stream::decodableHere();
    for (const stream::VideoCodec codec : stream::hardwareCodecs) {
        if (!stream::holds(both, codec)) {
            std::printf("stream: %s is not coded and decoded here\n",
                        std::string{stream::nameOf(codec)}.c_str());
            continue;
        }
        constexpr std::uint64_t rate = 20'000'000;
        const std::unique_ptr<stream::VideoEncoder> encoder = stream::openEncoder(codec);
        const std::unique_ptr<stream::VideoDecoder> decoder = stream::openDecoder(codec);
        expect(!decoder->intraOnly(), "a hardware codec's frames build on the ones before");
        Nv12 source = testPicture(320, 192);
        std::size_t decodedFrames = 0;
        double worst = 0;
        bool lostOne = false;
        bool heldAfterLoss = true;
        for (std::uint32_t number = 1; number <= 40; ++number) {
            // The picture scrolls, so each frame differs from the last.
            std::rotate(source.luma.begin(), source.luma.begin() + 3, source.luma.end());
            const bool key = number == 1 || number == 30;
            const stream::CodedFrame coded = encoder->encode(captured(source), rate, key);
            expect(coded.key == key, "a key frame comes when asked for, and only then");
            std::vector<stream::wire::VideoPacket> packets =
                packetsOf(coded, number, source, codec);
            if (number == 20 && packets.size() > 1) {
                packets.erase(packets.begin());
                lostOne = true;
            }
            if (number == 25) {
                expect(decoder->awaitingKey(), "a loss leaves the decoder waiting for a key frame");
            }
            for (const stream::wire::VideoPacket& packet : packets) {
                if (!decoder->push(packet)) {
                    continue;
                }
                stream::Picture decoded;
                if (!decoder->decode(decoded, stream::PictureMemory::Cpu)) {
                    continue;
                }
                ++decodedFrames;
                if (lostOne && number >= 20 && number < 30) {
                    heldAfterLoss = false;
                }
                // The hardware codec gives a frame as it is coded, no frame behind.
                worst = std::max(worst, meanError(source, decoded));
            }
        }
        expect(!decoder->awaitingKey(), "the key frame after the loss ends the wait");
        std::printf("stream: %s decoded %zu of 40 frames, worst mean error %.3f\n",
                    std::string{stream::nameOf(codec)}.c_str(), decodedFrames, worst);
        expect(heldAfterLoss, "after a loss nothing decodes until the next key frame");
        expect(decodedFrames >= 29, "every other frame decodes");
        expect(worst < 6.0, "each is the picture coded, to within the codec's loss");
    }
}

/// The guest received `datagrams` of `sent` the host sealed, each `bytes` long, by `atUs`.
stream::wire::Report reported(std::uint64_t sent, std::uint64_t datagrams, std::uint64_t bytes,
                              stream::VideoCodecs decoders) {
    return stream::wire::Report{.decoders = decoders,
                                .datagrams = datagrams,
                                .bytes = datagrams * bytes,
                                .newest = sent,
                                .probeBurst = 0,
                                .probeDatagrams = 0,
                                .probeQueueUs = 0};
}

/// A probe as the guest sees it while `drive` runs: its burst, the datagrams sent and its rate now.
struct Probed {
    std::uint32_t burst{0};
    std::uint32_t sent{0};
    std::uint64_t bitsPerSecond{0};
};

/// Drives `link`'s probe from `fromUs` to `toUs` in 1 ms steps, sending what it asks for, and every
/// 20 ms hears `report` of what was sent so far: the rates the probe asked for, the last first.
template <typename Report>
std::vector<std::uint64_t> drive(stream::LinkRate& link, std::uint64_t fromUs, std::uint64_t toUs,
                                 const Report& report) {
    std::vector<std::uint64_t> rates;
    std::uint32_t sent = 0;
    std::uint32_t burst = 0;
    double owed = 0;
    for (std::uint64_t now = fromUs; now <= toUs; now += 1000) {
        if (const std::optional<stream::LinkRate::Probing> probing = link.probe(now)) {
            burst = probing->burst;
            rates.push_back(probing->bitsPerSecond);
            owed += static_cast<double>(probing->bitsPerSecond) / 8 / 1000 /
                    static_cast<double>(stream::probeDatagramBytes);
            const auto count = static_cast<std::uint32_t>(owed);
            owed -= count;
            sent += count;
            link.probed(burst, count);
        }
        if ((now - fromUs) % 20'000 == 0 && burst != 0) {
            link.report(report(Probed{.burst = burst,
                                      .sent = sent,
                                      .bitsPerSecond = rates.empty() ? 0 : rates.back()}),
                        now);
        }
    }
    std::ranges::reverse(rates);
    return rates;
}

/// A frame of 130 packets over three blocks, sent to a decoder's frames without those in `lost`.
bool arrivesWhole(stream::PredictedFrames& frames, std::uint32_t number,
                  const std::set<std::size_t>& lost) {
    std::vector<std::string> parts;
    for (std::size_t i = 0; i < 130; ++i) {
        std::string part(stream::videoPacketBytes, '\0');
        for (std::size_t at = 0; at < part.size(); ++at) {
            part[at] = static_cast<char>((at * 31 + i * 7 + number) % 251);
        }
        parts.push_back(std::move(part));
    }
    parts.back().resize(77);
    const std::vector<std::string> parity = stream::parityOf(parts);
    expect(parity.size() == stream::parityFor(parts.size()), "parity is as long as promised");
    stream::wire::VideoPacket packet{.frame = number,
                                     .width = 2,
                                     .height = 2,
                                     .key = number == 1,
                                     .parts = static_cast<std::uint16_t>(parts.size()),
                                     .parity = static_cast<std::uint16_t>(parity.size()),
                                     .lastBytes = 77,
                                     .data = {}};
    bool whole = false;
    for (std::size_t i = 0; i < parts.size() + parity.size(); ++i) {
        packet.part = static_cast<std::uint16_t>(i);
        packet.data = i < parts.size() ? parts[i] : parity[i - parts.size()];
        if (!lost.contains(i) && frames.push(packet)) {
            whole = true;
        }
    }
    std::string all;
    for (const std::string& part : parts) {
        all += part;
    }
    return whole && frames.coded() == all;
}

void lostPacketsComeBackFromParity() {
    stream::PredictedFrames frames;
    expect(arrivesWhole(frames, 1, {}), "a frame nothing was lost from is whole");
    std::set<std::size_t> lost{64, 129};
    for (std::size_t i = 0; i < 13; ++i) {
        lost.insert(i * 4);
    }
    expect(arrivesWhole(frames, 2, lost), "as much as a block's parity comes back, the last too");
    expect(!frames.awaitingKey(), "and the next frame still has its reference");
    lost.insert(1);
    expect(!arrivesWhole(frames, 3, lost), "one packet more than the parity is a frame lost");
    expect(!arrivesWhole(frames, 4, {}), "and what follows waits for a key frame");
    expect(frames.awaitingKey(), "which the decoder asks for");
}

void aCalmPictureKeepsItsRateAfterALoss() {
    using stream::VideoCodec;
    const stream::VideoCodecs hevc = stream::codecBit(VideoCodec::Hevc);
    stream::LinkRate link{hevc};
    const std::vector<std::uint64_t> probed = drive(link, 0, 1'000'000, [&](const Probed& burst) {
        stream::wire::Report back = reported(burst.sent, burst.sent, 0, hevc);
        back.probeBurst = burst.burst;
        back.probeDatagrams = burst.sent;
        return back;
    });
    expect(!probed.empty() && link.plan() && link.plan()->bitsPerSecond == stream::hardwareTop,
           "a clean opening probe plans the hardware codec's top rate");
    std::uint64_t now = 3'000'000;
    std::uint64_t sent = 0;
    std::uint64_t received = 0;
    for (int tick = 0; tick < 40; ++tick) {
        now += 100'000;
        sent += 10;
        received += 9;
        link.report(reported(sent, received, received * 100, hevc), now);
    }
    const std::optional<stream::LinkPlan> plan = link.plan();
    expect(
        plan && plan->bitsPerSecond >= stream::hardwareTop / 3,
        "a loss on a calm picture backs off from the plan, not down to what it happened to send");
}

void theLinkPicksTheCodec() {
    using stream::VideoCodec;
    const stream::VideoCodecs all = stream::codecBit(VideoCodec::PyroWave) |
                                    stream::codecBit(VideoCodec::Hevc) |
                                    stream::codecBit(VideoCodec::H264);
    // The opening probe: sent as asked, and the guest's word on it once it is over.
    const auto opening = [](stream::LinkRate& link, double share, stream::VideoCodecs decoders) {
        constexpr std::uint64_t start = 1'000'000;
        constexpr std::uint64_t end = start + 760'000;
        std::uint32_t heard = 0;
        drive(link, start, end, [&](const Probed& probed) {
            heard = static_cast<std::uint32_t>(probed.sent * share);
            return stream::wire::Report{.decoders = decoders,
                                        .datagrams = heard,
                                        .bytes = 0,
                                        .newest = probed.sent,
                                        .probeBurst = probed.burst,
                                        .probeDatagrams = heard,
                                        .probeQueueUs = 0,
                                        .pictureWidth = 0,
                                        .pictureHeight = 0,
                                        .framesPerSecond = 0};
        });
        expect(heard > 0, "the opening probe is sent");
        return end;
    };

    // The plan's codec and rate, none planned reading as H.264 at 0.
    const auto planned = [](const stream::LinkRate& link) {
        return link.plan().value_or(
            stream::LinkPlan{.codec = VideoCodec::H264, .bitsPerSecond = 0});
    };
    stream::LinkRate wired{all};
    expect(!wired.plan(), "nothing is coded before the link was probed");
    opening(wired, 1.0, all);
    expect(planned(wired).codec == VideoCodec::PyroWave &&
               planned(wired).bitsPerSecond >= stream::pyroWaveFloor,
           "a link that carries the probe whole gets PyroWave");

    stream::LinkRate wifi{all};
    opening(wifi, 0.3, all);
    expect(planned(wifi).codec == VideoCodec::Hevc && planned(wifi).bitsPerSecond > 0 &&
               planned(wifi).bitsPerSecond <= stream::hardwareTop,
           "one that carries a third of it gets the best hardware codec both ends have");

    stream::LinkRate pyroWaveOnly{stream::codecBit(VideoCodec::PyroWave)};
    opening(pyroWaveOnly, 0.3, all);
    expect(planned(pyroWaveOnly).codec == VideoCodec::PyroWave,
           "with no hardware codec on both ends PyroWave codes anyway");

    // Running: a link that drops a tenth of what is sent is over its rate.
    stream::LinkRate running{all};
    std::uint64_t now = opening(running, 1.0, all);
    const std::uint64_t first = planned(running).bitsPerSecond;
    std::uint64_t sent = 0;
    std::uint64_t received = 0;
    for (int second = 0; second < 4; ++second) {
        for (int tick = 0; tick < 10; ++tick) {
            now += 100'000;
            sent += 1000;
            received += 900;
            running.report(reported(sent, received, 1428, all), now);
        }
    }
    expect(planned(running).codec == VideoCodec::Hevc,
           "a link that loses frames at PyroWave's rate falls back to a hardware codec");
    expect(running.summary().find("switches=1") != std::string::npos, "and the switch is counted");
    const std::uint64_t hardwareRate = planned(running).bitsPerSecond;

    // A probe on the hardware codec ramps up from the plan's rate; on a 60 Mbit/s link its queue
    // grows once it passes that, and it stops there with the plan kept.
    constexpr std::uint64_t capacity = 60'000'000;
    const auto queued = [&](const Probed& probed) {
        stream::wire::Report back = reported(sent, received, 1428, all);
        back.probeBurst = probed.burst;
        back.probeDatagrams = probed.sent;
        back.probeQueueUs = probed.bitsPerSecond > capacity ? 5'000 : 0;
        return back;
    };
    now += 11'000'000;
    const std::vector<std::uint64_t> stopped = drive(running, now, now + 1'500'000, queued);
    expect(!stopped.empty() && stopped.back() >= hardwareRate * 99 / 100 &&
               stopped.back() <= hardwareRate * 101 / 100,
           "a hardware codec's link is probed again, from the plan's rate");
    expect(stopped.front() > capacity && stopped.front() < capacity * 6 / 5 &&
               planned(running).codec == VideoCodec::Hevc,
           "and a probe whose queue grows stops just past the link's rate, the plan kept");
    std::printf("stream: a probe from %.0f Mbit/s stopped at %.0f Mbit/s after %zu ms\n",
                static_cast<double>(stopped.back()) / 1e6,
                static_cast<double>(stopped.front()) / 1e6, stopped.size());

    // The next, twice as far off, on a link with room: it reaches PyroWave's rate and PyroWave
    // codes again.
    now += 21'000'000;
    const auto whole = [&](const Probed& probed) {
        stream::wire::Report back = reported(sent, received, 1428, all);
        back.probeBurst = probed.burst;
        back.probeDatagrams = probed.sent;
        return back;
    };
    const std::vector<std::uint64_t> passed = drive(running, now, now + 2'500'000, whole);
    expect(!passed.empty() && passed.front() > stream::pyroWaveFloor &&
               planned(running).codec == VideoCodec::PyroWave,
           "and once a probe reaches PyroWave's rate with the queue still short PyroWave codes "
           "again");
    std::printf("stream: link %s, first %.0f Mbit/s\n", running.summary().c_str(),
                static_cast<double>(first) / 1e6);

    stream::LinkRate refused{all};
    opening(refused, 0.3, all);
    refused.refuse(VideoCodec::Hevc);
    expect(planned(refused).codec == VideoCodec::H264 && planned(refused).bitsPerSecond > 0,
           "a codec that cannot open gives way to the next");
}

/// A capped link carries its rate, holds what comes faster in a 20 ms queue and drops what that
/// has no room for.
void theCapQueuesAndDropsWhatTheRateHasNoRoomFor() {
    stream::LinkCap cap;
    const std::string datagram(1428, 'x');
    expect(cap.push(datagram, 1) && cap.pop(1).has_value(),
           "with no cap everything goes through at once");
    constexpr std::uint64_t rate = 8'000'000;
    cap.set(rate);
    std::uint64_t carried = 0;
    std::uint64_t longestUs = 0;
    std::deque<std::uint64_t> queuedAt;
    // Twice the rate offered for a second.
    for (std::uint64_t now = 1; now <= 1'000'000; now += 714) {
        if (cap.push(datagram, now)) {
            queuedAt.push_back(now);
        }
        while (const std::optional<stream::LinkCap::Out> out = cap.pop(now)) {
            carried += out->datagram.size() * 8;
            longestUs = std::max(longestUs, out->leftUs - queuedAt.front());
            queuedAt.pop_front();
        }
    }
    std::printf("stream: an 8 Mbit/s cap carried %.2f Mbit in a second, queued up to %.1f ms\n",
                static_cast<double>(carried) / 1e6, static_cast<double>(longestUs) / 1000);
    expect(static_cast<double>(carried) > rate * 0.93 && static_cast<double>(carried) < rate * 1.05,
           "a cap carries its rate and no more");
    expect(longestUs > 15'000 && longestUs <= 21'000, "and what waits, waits its 20 ms queue");
    cap.set(0);
    std::optional<stream::LinkCap::Out> last;
    expect(cap.push(datagram, 3'000'000), "lifting it takes everything");
    while (std::optional<stream::LinkCap::Out> out = cap.pop(3'000'000)) {
        last = std::move(out);
    }
    expect(last && last->leftUs == 3'000'000, "and lets it through at once");
}

/// A 440 Hz tone through Opus, one packet lost on the way: the guest plays every frame, the lost
/// one filled in, and the tone comes out.
void soundSurvivesTheCodec() {
    stream::AudioEncoder encoder;
    constexpr std::size_t frames = 50;
    constexpr double pitch = 440.0;
    constexpr double level = 0.5;
    std::vector<std::string> packets;
    std::vector<float> frame(stream::audioFrame * stream::audioChannels);
    for (std::size_t n = 0; n < frames; ++n) {
        for (std::size_t i = 0; i < stream::audioFrame; ++i) {
            const double t = static_cast<double>(n * stream::audioFrame + i) / stream::audioRate;
            const auto sample =
                static_cast<float>(level * std::sin(2 * std::numbers::pi * pitch * t));
            frame[2 * i] = sample;
            frame[2 * i + 1] = sample;
        }
        packets.push_back(encoder.encode(frame));
    }

    std::size_t played = 0;
    double energy = 0.0;
    stream::GuestAudio guest{[&](std::span<const float> samples) {
        expect(samples.size() == stream::audioFrame * stream::audioChannels,
               "each frame plays whole");
        ++played;
        for (const float sample : samples) {
            energy += static_cast<double>(sample) * sample;
        }
    }};
    for (std::size_t n = 0; n < frames; ++n) {
        if (n == frames / 2) {
            continue;
        }
        guest.take(stream::wire::AudioPacket{.sequence = static_cast<std::uint32_t>(n + 1),
                                             .capturedUs = 1000,
                                             .data = packets[n]},
                   1500, 0);
    }
    expect(played == frames, "a lost packet is filled in");
    const double rms = std::sqrt(energy / static_cast<double>(frames * frame.size()));
    std::printf("stream: sound rms %.3f, %s\n", rms, guest.summary().c_str());
    expect(rms > 0.25 && rms < 0.45, "the tone comes out at its level");
    expect(guest.summary().starts_with("packets=49 lost=1 arrival=0.50/0.50"),
           "the guest counts what came, what was lost and how long it took");
}

/// A bucket full to its depth lets that much through at once, then its rate: what the host's
/// pacer holds a frame's tail back by.
void theBucketHoldsBackWhatItsRateHasNoRoomFor() {
    stream::TokenBucket bucket;
    bucket.set(1'000'000, 10'000);
    expect(bucket.take(10'000, 5), "a full bucket lets its depth through at once");
    expect(!bucket.take(1'000, 5), "and nothing more then");
    expect(bucket.waitUs(1'000, 5) == 1'000, "a kilobyte more waits a millisecond at 1 MB/s");
    expect(bucket.take(1'000, 1'005), "and goes once it has");
    expect(!bucket.take(20'000, 10'000'000), "it never holds more than its depth");
}

void thisSystemReachesWhatItBuilds() {
    expect(stream::playingMissing().empty(), "every platform plays a host's game");
#ifdef __linux__
    expect(stream::hostingMissing().empty(), "Linux hosts");
#else
    expect(!stream::hostingMissing().empty(), "and only Linux has a screen to stream");
#endif
}

} // namespace

int main(int argc, char** argv) try {
    if (argc == 3 && std::string_view{argv[1]} == "--write-coded-picture") {
        writeCodedPicture(argv[2]);
        return 0;
    }
    datagramsOpenOnce();
    payloadsReadAsWritten();
    aSessionCarriesInputAndEnds();
    noCommonCodecEndsTheSession();
    lostPacketsComeBackFromParity();
    aCalmPictureKeepsItsRateAfterALoss();
    theHostsEndReachesTheGuest();
    aPictureCodedElsewhereDecodes();
#ifndef __APPLE__
    picturesSurviveTheCodec();
#endif
    picturesSurviveTheHardwareCodecs();
    theLinkPicksTheCodec();
    theCapQueuesAndDropsWhatTheRateHasNoRoomFor();
    theBucketHoldsBackWhatItsRateHasNoRoomFor();
    thisSystemReachesWhatItBuilds();
    soundSurvivesTheCodec();
    std::printf("stream: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
