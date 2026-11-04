// stream: remote play's sealed datagrams, their payloads, and a host and guest session over
// loopback UDP.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "lan/pairing_keys.hpp"
#include "stream/audio_codec.hpp"
#include "stream/guest_audio.hpp"
#include "stream/guest_stream.hpp"
#include "stream/host_stream.hpp"
#include "stream/host_video.hpp"
#include "stream/packet_seal.hpp"
#include "stream/stream_wire.hpp"
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
                                  .capturedUs = 1,
                                  .encodedUs = 2,
                                  .sentUs = 3,
                                  .data = "bits"};
    const std::optional<wire::VideoPacket> readVideo = wire::readVideoPacket(wire::write(video));
    expect(readVideo && readVideo->frame == 9 && readVideo->width == 1280 &&
               readVideo->height == 720 && readVideo->capturedUs == 1 &&
               readVideo->encodedUs == 2 && readVideo->sentUs == 3 && readVideo->data == "bits",
           "a video packet reads as written");
    expect(!wire::readVideoPacket(wire::write(video).substr(0, wire::videoHeaderBytes)),
           "one with no data does not");

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
    stream::HostStream host{stream::HostStream::Setup{.keys = keysFor(3),
                                                      .guest = "Desk",
                                                      .pad =
                                                          [&](const stream::wire::PadState& pad) {
                                                              const std::lock_guard lock{mutex};
                                                              heard = pad;
                                                          },
                                                      .keyboard = {},
                                                      .mouse = {}}};
    const std::optional<stream::UdpAddress> address = stream::udpAddress("127.0.0.1", host.port());
    if (!address || host.port() == 0) {
        fail("the host takes packets on a port of its own");
    }

    // Someone else's session is not heard; the host waits for its own guest.
    stream::GuestStream stranger{
        stream::GuestStream::Setup{.host = *address, .keys = keysFor(4), .video = {}, .audio = {}}};
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
            },
        .audio = {}}};
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

    host.send(PacketKind::Video, stream::wire::write(stream::wire::VideoPacket{
                                     .frame = 1, .width = 2, .height = 2, .data = "frame"}));
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
    stream::HostStream host{stream::HostStream::Setup{
        .keys = keysFor(5), .guest = "Desk", .pad = {}, .keyboard = {}, .mouse = {}}};
    const std::optional<stream::UdpAddress> address = stream::udpAddress("127.0.0.1", host.port());
    if (!address) {
        fail("the host takes packets on a port of its own");
    }
    stream::GuestStream guest{
        stream::GuestStream::Setup{.host = *address, .keys = keysFor(5), .video = {}, .audio = {}}};
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

void picturesSurviveTheCodec() {
    const Nv12 source = testPicture(256, 144);
    const stream::CapturedFrame frame{.width = source.width,
                                      .height = source.height,
                                      .stride = source.stride,
                                      .luma = source.luma,
                                      .chroma = source.chroma,
                                      .capturedUs = 0};
    stream::VideoEncoder encoder;
    const std::vector<std::string> packets = encoder.encode(frame, stream::frameBudget);
    expect(packets.size() > 1, "a picture is cut into several packets");
    for (const std::string& packet : packets) {
        expect(!packet.empty() && packet.size() <= stream::videoPacketBytes,
               "each fits one datagram");
    }

    stream::VideoDecoder decoder;
    for (std::size_t i = 0; i + 1 < packets.size(); ++i) {
        expect(!decoder.push(source.width, source.height, packets[i]),
               "a frame is not whole before its last packet");
    }
    expect(decoder.push(source.width, source.height, packets.back()),
           "and is once all of them came");
    stream::Picture decoded;
    decoder.decode(decoded);
    expect(decoded.width == 256 && decoded.height == 144 &&
               decoded.rgba.size() == std::size_t{256} * 144 * 4,
           "it decodes at its own size");

    // The source as the decoder's planes, through the same colour conversion.
    std::vector<std::uint8_t> luma;
    std::vector<std::uint8_t> blue;
    std::vector<std::uint8_t> red;
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            luma.push_back(source.luma[static_cast<std::size_t>(y) * source.stride +
                                       static_cast<std::size_t>(x)]);
        }
    }
    for (int y = 0; y < source.height / 2; ++y) {
        for (int x = 0; x < source.width / 2; ++x) {
            const std::size_t at =
                static_cast<std::size_t>(y) * source.stride + 2 * static_cast<std::size_t>(x);
            blue.push_back(source.chroma[at]);
            red.push_back(source.chroma[at + 1]);
        }
    }
    std::vector<std::uint8_t> expected;
    stream::rgbaFromYcbcr(
        stream::YcbcrPlanes{
            .luma = luma, .blue = blue, .red = red, .width = source.width, .height = source.height},
        expected);
    double error = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        error += std::abs(static_cast<int>(expected[i]) - static_cast<int>(decoded.rgba[i]));
    }
    error /= static_cast<double>(expected.size());
    std::printf("stream: codec mean error %.3f over %zu packets\n", error, packets.size());
    expect(error < 2.0, "the decoded picture is the one coded, to within the codec's loss");

    const std::vector<std::string> next = encoder.encode(frame, stream::frameBudget);
    for (std::size_t i = 1; i < next.size(); ++i) {
        expect(!decoder.push(source.width, source.height, next[i]),
               "a frame missing a packet is never whole");
    }
    const std::vector<std::string> after = encoder.encode(frame, stream::frameBudget);
    bool whole = false;
    for (const std::string& packet : after) {
        whole = decoder.push(source.width, source.height, packet);
    }
    expect(whole, "and the next frame drops it and decodes");
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

} // namespace

int main() try {
    datagramsOpenOnce();
    payloadsReadAsWritten();
    aSessionCarriesInputAndEnds();
    theHostsEndReachesTheGuest();
    picturesSurviveTheCodec();
    soundSurvivesTheCodec();
    std::printf("stream: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
