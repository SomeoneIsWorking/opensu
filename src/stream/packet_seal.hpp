// packet_seal — remote play's datagrams, each sealed on its own: a clear header (version, kind and
// the sender's packet counter), then the payload under ChaCha20-Poly1305 with the session's key for
// that sender (`lan::streamKeys`) and the counter as its nonce, the header authenticated with it.
// The opener takes each counter once: anything older than its window, or already seen, is dropped,
// so a datagram heard on the LAN cannot be played again.
#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::stream {

/// What a datagram carries; the payload's layout is `stream_wire`'s.
enum class PacketKind : std::uint8_t {
    /// The guest's keepalive with its clock, which the host answers with `Pong`.
    Ping = 1,
    Pong = 2,
    /// One packet of a coded frame.
    Video = 3,
    /// One Opus packet.
    Audio = 4,
    /// The guest's pad as it stands.
    Pad = 5,
    /// The keys the guest holds.
    Keys = 6,
    /// The guest's pointer.
    Mouse = 7,
    /// The session is over, and why.
    End = 8,
    /// Filler the host sends to measure what the link carries.
    Probe = 9,
    /// What the guest received, and the codecs it decodes.
    Report = 10,
    /// The guest lost a frame its codec needs, and asks for a key frame.
    KeyFrame = 11,
    /// The guest's mapping for the remote pad.
    Mapping = 12,
};

/// Bytes a sealed datagram adds to its payload: the header and the tag.
inline constexpr std::size_t headerBytes = 10;
inline constexpr std::size_t sealOverhead = headerBytes + 16;

class PacketSeal {
  public:
    /// Seals with `key`, counting from 0.
    explicit PacketSeal(std::span<const std::uint8_t> key);
    ~PacketSeal();
    PacketSeal(const PacketSeal&) = delete;
    PacketSeal& operator=(const PacketSeal&) = delete;
    PacketSeal(PacketSeal&&) = delete;
    PacketSeal& operator=(PacketSeal&&) = delete;

    /// `payload` as the next datagram of `kind`. Not thread safe.
    [[nodiscard]] std::string seal(PacketKind kind, std::string_view payload);

  private:
    std::vector<std::uint8_t> key_;
    std::uint64_t counter_{0};
};

/// A datagram that opened.
struct Opened {
    PacketKind kind;
    std::uint64_t counter{0};
    std::string payload;
};

class PacketOpener {
  public:
    /// How many counters behind the newest one are still taken: a frame's packets, reordered.
    static constexpr std::size_t window = 4096;

    /// Opens what the other side sealed with `key`.
    explicit PacketOpener(std::span<const std::uint8_t> key);
    ~PacketOpener();
    PacketOpener(const PacketOpener&) = delete;
    PacketOpener& operator=(const PacketOpener&) = delete;
    PacketOpener(PacketOpener&&) = delete;
    PacketOpener& operator=(PacketOpener&&) = delete;

    /// What `datagram` holds; nothing when it was not sealed with the key, was changed, or its
    /// counter was taken already or is older than the window. Not thread safe.
    [[nodiscard]] std::optional<Opened> open(std::string_view datagram);

  private:
    /// Whether `counter` is new, without taking it.
    [[nodiscard]] bool fresh(std::uint64_t counter) const;
    void take(std::uint64_t counter);

    std::vector<std::uint8_t> key_;
    /// The newest counter taken, and which of the `window` before it were.
    std::optional<std::uint64_t> newest_;
    std::bitset<window> seen_;
};

} // namespace opensu::stream
