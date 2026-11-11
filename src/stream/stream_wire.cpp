#include "stream_wire.hpp"

#include <algorithm>
#include <chrono>
#include <set>

#include "fileio/little_endian.hpp"

namespace opensu::stream::wire {
namespace {

using fileio::appendLittleEndian;
using fileio::littleEndian;

/// The longest reason an End carries.
constexpr std::size_t longestReason = 512;

/// Reads fields in order from a payload; `ok` turns false once one runs past its end.
class Reader {
  public:
    explicit Reader(std::string_view payload) : payload_{payload} {
    }

    std::uint64_t take(std::size_t size) {
        if (at_ + size > payload_.size()) {
            ok_ = false;
            return 0;
        }
        const std::uint64_t value = littleEndian(payload_, at_, size);
        at_ += size;
        return value;
    }
    std::int16_t signed16() {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(take(2)));
    }
    std::string_view rest() {
        const std::string_view tail = payload_.substr(std::min(at_, payload_.size()));
        at_ = payload_.size();
        return tail;
    }
    [[nodiscard]] bool ok() const noexcept {
        return ok_;
    }
    /// Whether every byte was read and none was missing.
    [[nodiscard]] bool whole() const noexcept {
        return ok_ && at_ == payload_.size();
    }

  private:
    std::string_view payload_;
    std::size_t at_{0};
    bool ok_{true};
};

void appendSigned16(std::string& out, std::int16_t value) {
    appendLittleEndian(out, static_cast<std::uint16_t>(value), 2);
}

std::uint32_t buttonBits(const std::set<gamepad::Button>& buttons) {
    std::uint32_t bits = 0;
    for (const gamepad::Button button : buttons) {
        bits |= 1U << static_cast<unsigned>(button);
    }
    return bits;
}

/// The pad buttons set in `bits`; any other bit is left out.
std::set<gamepad::Button> buttonsOf(std::uint64_t bits) {
    std::set<gamepad::Button> buttons;
    for (const gamepad::Button button : gamepad::padButtons) {
        if ((bits >> static_cast<unsigned>(button) & 1U) != 0) {
            buttons.insert(button);
        }
    }
    return buttons;
}

/// The pad button numbered `value`, or nothing for one the virtual pad does not carry.
std::optional<gamepad::Button> padButton(std::uint64_t value) {
    const auto found = std::ranges::find_if(gamepad::padButtons, [value](gamepad::Button button) {
        return static_cast<std::uint64_t>(button) == value;
    });
    return found == gamepad::padButtons.end() ? std::nullopt : std::optional{*found};
}

} // namespace

std::uint64_t nowMicros() noexcept {
    return microsOf(std::chrono::steady_clock::now());
}

std::uint64_t microsOf(std::chrono::steady_clock::time_point time) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count());
}

std::string write(const Ping& ping) {
    std::string out;
    appendLittleEndian(out, ping.sentUs, 8);
    return out;
}

std::optional<Ping> readPing(std::string_view payload) {
    Reader in{payload};
    const Ping ping{in.take(8)};
    return in.whole() ? std::optional{ping} : std::nullopt;
}

std::string write(const Pong& pong) {
    std::string out;
    appendLittleEndian(out, pong.pingUs, 8);
    appendLittleEndian(out, pong.hostUs, 8);
    return out;
}

std::optional<Pong> readPong(std::string_view payload) {
    Reader in{payload};
    Pong pong;
    pong.pingUs = in.take(8);
    pong.hostUs = in.take(8);
    return in.whole() ? std::optional{pong} : std::nullopt;
}

std::string write(const VideoPacket& packet) {
    std::string out;
    out.reserve(videoHeaderBytes + packet.data.size());
    appendLittleEndian(out, packet.frame, 4);
    appendLittleEndian(out, packet.width, 2);
    appendLittleEndian(out, packet.height, 2);
    out.push_back(static_cast<char>(packet.codec));
    out.push_back(static_cast<char>(packet.key ? 1 : 0));
    appendLittleEndian(out, packet.part, 2);
    appendLittleEndian(out, packet.parts, 2);
    appendLittleEndian(out, packet.parity, 2);
    appendLittleEndian(out, packet.lastBytes, 2);
    appendLittleEndian(out, packet.capturedUs, 8);
    appendLittleEndian(out, packet.encodedUs, 8);
    appendLittleEndian(out, packet.sentUs, 8);
    out += packet.data;
    return out;
}

std::optional<VideoPacket> readVideoPacket(std::string_view payload) {
    Reader in{payload};
    VideoPacket packet;
    packet.frame = static_cast<std::uint32_t>(in.take(4));
    packet.width = static_cast<std::uint16_t>(in.take(2));
    packet.height = static_cast<std::uint16_t>(in.take(2));
    packet.codec = static_cast<std::uint8_t>(in.take(1));
    const std::uint64_t flags = in.take(1);
    packet.key = flags == 1;
    packet.part = static_cast<std::uint16_t>(in.take(2));
    packet.parts = static_cast<std::uint16_t>(in.take(2));
    packet.parity = static_cast<std::uint16_t>(in.take(2));
    packet.lastBytes = static_cast<std::uint16_t>(in.take(2));
    packet.capturedUs = in.take(8);
    packet.encodedUs = in.take(8);
    packet.sentUs = in.take(8);
    packet.data = in.rest();
    if (!in.ok() || flags > 1 || packet.data.empty() || packet.width == 0 || packet.height == 0 ||
        packet.parts == 0 || packet.part >= packet.parts + packet.parity) {
        return std::nullopt;
    }
    return packet;
}

std::string write(const Probe& probe, std::size_t bytes) {
    std::string out;
    appendLittleEndian(out, probe.burst, 4);
    appendLittleEndian(out, probe.index, 4);
    appendLittleEndian(out, probe.sentUs, 8);
    out.resize(std::max(bytes, out.size()), '\0');
    return out;
}

std::optional<Probe> readProbe(std::string_view payload) {
    Reader in{payload};
    Probe probe;
    probe.burst = static_cast<std::uint32_t>(in.take(4));
    probe.index = static_cast<std::uint32_t>(in.take(4));
    probe.sentUs = in.take(8);
    return in.ok() ? std::optional{probe} : std::nullopt;
}

std::string write(const Report& report) {
    std::string out;
    out.push_back(static_cast<char>(report.decoders));
    appendLittleEndian(out, report.datagrams, 8);
    appendLittleEndian(out, report.bytes, 8);
    appendLittleEndian(out, report.newest, 8);
    appendLittleEndian(out, report.probeBurst, 4);
    appendLittleEndian(out, report.probeDatagrams, 4);
    appendLittleEndian(out, report.probeQueueUs, 4);
    appendLittleEndian(out, report.pictureWidth, 2);
    appendLittleEndian(out, report.pictureHeight, 2);
    out.push_back(static_cast<char>(report.framesPerSecond));
    return out;
}

std::optional<Report> readReport(std::string_view payload) {
    Reader in{payload};
    Report report;
    report.decoders = static_cast<std::uint8_t>(in.take(1));
    report.datagrams = in.take(8);
    report.bytes = in.take(8);
    report.newest = in.take(8);
    report.probeBurst = static_cast<std::uint32_t>(in.take(4));
    report.probeDatagrams = static_cast<std::uint32_t>(in.take(4));
    report.probeQueueUs = static_cast<std::uint32_t>(in.take(4));
    report.pictureWidth = static_cast<std::uint16_t>(in.take(2));
    report.pictureHeight = static_cast<std::uint16_t>(in.take(2));
    report.framesPerSecond = static_cast<std::uint8_t>(in.take(1));
    return in.whole() ? std::optional{report} : std::nullopt;
}

std::string write(const PadMapping& mapping) {
    std::string out;
    appendLittleEndian(out, static_cast<std::uint64_t>(mapping.holdDelay.count()), 4);
    out.push_back(static_cast<char>(mapping.map.buttons.size()));
    for (const auto& [button, outputs] : mapping.map.buttons) {
        out.push_back(static_cast<char>(button));
        appendLittleEndian(out, buttonBits(outputs), 4);
    }
    out.push_back(static_cast<char>(mapping.map.chords.size()));
    for (const auto& [chord, outputs] : mapping.map.chords) {
        out.push_back(static_cast<char>(chord.modifier));
        out.push_back(static_cast<char>(chord.trigger));
        appendLittleEndian(out, buttonBits(outputs), 4);
    }
    return out;
}

std::optional<PadMapping> readPadMapping(std::string_view payload) {
    Reader in{payload};
    PadMapping mapping;
    mapping.holdDelay = std::chrono::milliseconds{in.take(4)};
    const std::uint64_t buttons = in.take(1);
    for (std::uint64_t i = 0; i < buttons && in.ok(); ++i) {
        const std::optional<gamepad::Button> button = padButton(in.take(1));
        const std::uint64_t outputs = in.take(4);
        if (!button) {
            return std::nullopt;
        }
        mapping.map.buttons[*button] = buttonsOf(outputs);
    }
    const std::uint64_t chords = in.take(1);
    for (std::uint64_t i = 0; i < chords && in.ok(); ++i) {
        const std::optional<gamepad::Button> modifier = padButton(in.take(1));
        const std::optional<gamepad::Button> trigger = padButton(in.take(1));
        const std::uint64_t outputs = in.take(4);
        if (!modifier || !trigger) {
            return std::nullopt;
        }
        mapping.map.chords[gamepad::Chord{*modifier, *trigger}] = buttonsOf(outputs);
    }
    return in.whole() ? std::optional{mapping} : std::nullopt;
}

std::string write(const AudioPacket& packet) {
    std::string out;
    appendLittleEndian(out, packet.sequence, 4);
    appendLittleEndian(out, packet.capturedUs, 8);
    out += packet.data;
    return out;
}

std::optional<AudioPacket> readAudioPacket(std::string_view payload) {
    Reader in{payload};
    AudioPacket packet;
    packet.sequence = static_cast<std::uint32_t>(in.take(4));
    packet.capturedUs = in.take(8);
    packet.data = in.rest();
    if (!in.ok() || packet.data.empty()) {
        return std::nullopt;
    }
    return packet;
}

std::string write(const PadState& pad) {
    std::string out;
    appendLittleEndian(out, pad.buttons, 4);
    appendSigned16(out, pad.leftX);
    appendSigned16(out, pad.leftY);
    appendSigned16(out, pad.rightX);
    appendSigned16(out, pad.rightY);
    out.push_back(static_cast<char>(pad.leftTrigger));
    out.push_back(static_cast<char>(pad.rightTrigger));
    return out;
}

std::optional<PadState> readPadState(std::string_view payload) {
    Reader in{payload};
    PadState pad;
    pad.buttons = static_cast<std::uint32_t>(in.take(4));
    pad.leftX = in.signed16();
    pad.leftY = in.signed16();
    pad.rightX = in.signed16();
    pad.rightY = in.signed16();
    pad.leftTrigger = static_cast<std::uint8_t>(in.take(1));
    pad.rightTrigger = static_cast<std::uint8_t>(in.take(1));
    return in.whole() ? std::optional{pad} : std::nullopt;
}

std::string write(const KeyState& keys) {
    const std::size_t count = std::min(keys.held.size(), heldKeys);
    std::string out;
    out.push_back(static_cast<char>(count));
    out.append(reinterpret_cast<const char*>(keys.held.data()), count);
    return out;
}

std::optional<KeyState> readKeyState(std::string_view payload) {
    Reader in{payload};
    const auto count = static_cast<std::size_t>(in.take(1));
    const std::string_view held = in.rest();
    if (!in.ok() || count > heldKeys || held.size() != count) {
        return std::nullopt;
    }
    KeyState keys;
    keys.held.assign(held.begin(), held.end());
    return keys;
}

std::string write(const MouseState& mouse) {
    std::string out;
    appendLittleEndian(out, mouse.x, 2);
    appendLittleEndian(out, mouse.y, 2);
    out.push_back(static_cast<char>(mouse.buttons));
    appendLittleEndian(out, static_cast<std::uint32_t>(mouse.wheel), 4);
    return out;
}

std::optional<MouseState> readMouseState(std::string_view payload) {
    Reader in{payload};
    MouseState mouse;
    mouse.x = static_cast<std::uint16_t>(in.take(2));
    mouse.y = static_cast<std::uint16_t>(in.take(2));
    mouse.buttons = static_cast<std::uint8_t>(in.take(1));
    mouse.wheel = static_cast<std::int32_t>(static_cast<std::uint32_t>(in.take(4)));
    return in.whole() ? std::optional{mouse} : std::nullopt;
}

std::string write(const End& end) {
    return end.reason.substr(0, longestReason);
}

std::optional<End> readEnd(std::string_view payload) {
    if (payload.size() > longestReason) {
        return std::nullopt;
    }
    return End{readText(payload)};
}

std::string readText(std::string_view payload) {
    std::string text{payload.substr(0, longestReason)};
    std::ranges::replace_if(
        text,
        [](char c) {
            return static_cast<unsigned char>(c) < ' ';
        },
        ' ');
    return text;
}

} // namespace opensu::stream::wire
