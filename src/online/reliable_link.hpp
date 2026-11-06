// reliable_link — ordered messages between two shells over sealed datagrams, for what must arrive
// before a game starts: an ask, its answer, the saves, the word to go. Acks, a window and resends,
// with keepalives that measure the round trip and notice a peer gone. There are only those kinds of
// message, each capped: a size past its kind's cap breaks the link before anything is allocated.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "stream/packet_seal.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::online {

enum class LinkKind : std::uint8_t {
    /// The ROM, its console and what must match (`lan::wire::NetplayAsk`).
    Ask = 1,
    /// Yes, or no and why.
    Answer = 2,
    /// One player's save.
    Save = 3,
    /// The session's clock and delay; both start.
    Go = 4,
};

/// The most a kind's body may be.
[[nodiscard]] std::size_t linkCap(LinkKind kind) noexcept;

/// A message that arrived whole.
struct LinkMessage {
    LinkKind kind;
    std::string body;
};

class ReliableLink {
  public:
    using Clock = std::chrono::steady_clock;

    /// The largest payload of one datagram, under a path's usual MTU with the seal around it.
    static constexpr std::size_t chunkBytes = 1100;
    /// How many datagrams may be unacknowledged, and held out of order.
    static constexpr std::size_t window = 32;

    /// Talks to `peer` on `socket`, which must outlive this.
    ReliableLink(const stream::UdpSocket& socket, stream::UdpAddress peer,
                 std::span<const std::uint8_t> sendKey, std::span<const std::uint8_t> receiveKey);

    /// Queues a message; `body` must fit `kind`'s cap.
    void send(LinkKind kind, std::string_view body);
    /// Sends what is due and takes what came, waiting at most `wait`.
    void pump(std::chrono::milliseconds wait);
    /// The next whole message.
    [[nodiscard]] std::optional<LinkMessage> take();
    /// Whether all that was sent is acknowledged.
    [[nodiscard]] bool flushed() const noexcept {
        return outgoing_.empty() && unacked_.empty();
    }
    /// Nothing has been heard for too long.
    [[nodiscard]] bool silent() const noexcept;
    /// The peer sent what no openSU sends.
    [[nodiscard]] bool broken() const noexcept {
        return broken_;
    }
    /// The last measured round trip.
    [[nodiscard]] std::optional<std::chrono::milliseconds> roundTrip() const noexcept {
        return roundTrip_;
    }

  private:
    struct Unacked {
        std::uint32_t sequence{0};
        std::string chunk;
        Clock::time_point sent;
    };

    void sendChunks();
    void sendData(std::uint32_t sequence, std::string_view chunk);
    void sendAck();
    void sendPing(Clock::time_point now);
    void takeDatagram(std::string_view payload);
    void takeData(std::string_view payload);
    void takeAck(std::string_view payload);
    void takePong(std::string_view payload);
    void parse();

    const stream::UdpSocket& socket_;
    stream::UdpAddress peer_;
    stream::PacketSeal seal_;
    stream::PacketOpener opener_;
    std::vector<char> buffer_;
    std::string outgoing_;
    std::deque<Unacked> unacked_;
    std::uint32_t nextSequence_{0};
    std::map<std::uint32_t, std::string> early_;
    std::uint32_t expected_{0};
    std::string incoming_;
    std::deque<LinkMessage> arrived_;
    Clock::time_point started_{Clock::now()};
    Clock::time_point heard_{started_};
    Clock::time_point pinged_{};
    std::optional<std::chrono::milliseconds> roundTrip_;
    bool broken_{false};
};

} // namespace opensu::online
