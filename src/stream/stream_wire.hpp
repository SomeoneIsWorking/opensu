// stream_wire — the payloads of remote play's datagrams (`PacketKind`), written and read, the one
// place each layout is. Integers are little-endian; times are microseconds on the sender's
// monotonic clock. Anything short or malformed reads as nothing. Pad, keys and pointer go as the
// whole state each time, so a lost datagram is mended by the next.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::stream::wire {

/// Now on the monotonic clock the times are on, in microseconds.
[[nodiscard]] std::uint64_t nowMicros() noexcept;
/// `time` on that clock, in microseconds.
[[nodiscard]] std::uint64_t microsOf(std::chrono::steady_clock::time_point time) noexcept;

/// A guest's keepalive: when it was sent.
struct Ping {
    std::uint64_t sentUs{0};
};
[[nodiscard]] std::string write(const Ping& ping);
[[nodiscard]] std::optional<Ping> readPing(std::string_view payload);

/// The host's answer: the ping's time and the host's clock as it answered.
struct Pong {
    std::uint64_t pingUs{0};
    std::uint64_t hostUs{0};
};
[[nodiscard]] std::string write(const Pong& pong);
[[nodiscard]] std::optional<Pong> readPong(std::string_view payload);

/// One PyroWave packet of frame `frame`, `width` by `height`. `capturedUs` is when the host took
/// the frame, `encodedUs` when the encoder had it and `sentUs` when its first packet went out.
struct VideoPacket {
    std::uint32_t frame{0};
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint64_t capturedUs{0};
    std::uint64_t encodedUs{0};
    std::uint64_t sentUs{0};
    std::string data;
};
[[nodiscard]] std::string write(const VideoPacket& packet);
[[nodiscard]] std::optional<VideoPacket> readVideoPacket(std::string_view payload);
/// The bytes `write` puts before a video packet's data.
inline constexpr std::size_t videoHeaderBytes = 4 + 2 + 2 + 8 + 8 + 8;

/// One Opus packet of 48 kHz stereo sound, `sequence` counting packets.
struct AudioPacket {
    std::uint32_t sequence{0};
    std::uint64_t capturedUs{0};
    std::string data;
};
[[nodiscard]] std::string write(const AudioPacket& packet);
[[nodiscard]] std::optional<AudioPacket> readAudioPacket(std::string_view payload);

/// The guest's pad: the buttons held (bit `gamepad::Button` value), the sticks on the virtual
/// pad's scale and the triggers' travel.
struct PadState {
    std::uint32_t buttons{0};
    std::int16_t leftX{0};
    std::int16_t leftY{0};
    std::int16_t rightX{0};
    std::int16_t rightY{0};
    std::uint8_t leftTrigger{0};
    std::uint8_t rightTrigger{0};

    bool operator==(const PadState&) const = default;
};
[[nodiscard]] std::string write(const PadState& pad);
[[nodiscard]] std::optional<PadState> readPadState(std::string_view payload);

/// The keys the guest holds, as SDL scancodes (USB HID usages); at most `heldKeys`.
inline constexpr std::size_t heldKeys = 16;
struct KeyState {
    std::vector<std::uint8_t> held;

    bool operator==(const KeyState&) const = default;
};
[[nodiscard]] std::string write(const KeyState& keys);
[[nodiscard]] std::optional<KeyState> readKeyState(std::string_view payload);

/// The guest's pointer over the picture, 0 to 65535 across each side; the buttons held (bit 0
/// left, 1 middle, 2 right) and the wheel's notches since the session began.
struct MouseState {
    std::uint16_t x{0};
    std::uint16_t y{0};
    std::uint8_t buttons{0};
    std::int32_t wheel{0};

    bool operator==(const MouseState&) const = default;
};
[[nodiscard]] std::string write(const MouseState& mouse);
[[nodiscard]] std::optional<MouseState> readMouseState(std::string_view payload);

/// Why the session ended, said to the other side.
struct End {
    std::string reason;
};
[[nodiscard]] std::string write(const End& end);
[[nodiscard]] std::optional<End> readEnd(std::string_view payload);

} // namespace opensu::stream::wire
