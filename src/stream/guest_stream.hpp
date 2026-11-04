// guest_stream — the guest's end of one remote-play session: a UDP socket of its own, the seal for
// what it sends and the opener for what the host sends, heard only from the host's address. It
// pings five times a second, which keeps the session alive and measures the round trip and how far
// the host's clock is from its own, and with each ping reports what it received (`wire::Report`)
// for the host's measure of the link; it sends the player's pad, keys, pointer and pad mapping as
// they change and again with each ping, so a lost datagram is mended, and asks for a key frame
// when the video says one is needed. The session is over when the host says so, goes quiet, or the
// guest ends it (`end`, told to the host). A test run may cap what it takes (`capLink`).
#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "lan/pairing_keys.hpp"
#include "link_cap.hpp"
#include "packet_seal.hpp"
#include "stream_wire.hpp"
#include "udp_socket.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

class GuestStream {
  public:
    struct Setup {
        UdpAddress host;
        lan::StreamKeys keys;
        /// What the host sends of the game, called on the stream's thread with the time it came
        /// and how far the host's clock is ahead of this one's then. The video returns whether
        /// it wants a key frame.
        std::function<bool(const wire::VideoPacket&, std::uint64_t receivedUs,
                           std::int64_t hostAheadUs)>
            video;
        std::function<void(const wire::AudioPacket&, std::uint64_t receivedUs,
                           std::int64_t hostAheadUs)>
            audio;
        /// The codecs this guest decodes.
        VideoCodecs decoders{0};
    };

    /// Binds a free UDP port and starts pinging the host; throws `std::system_error`.
    explicit GuestStream(Setup setup);
    ~GuestStream();
    GuestStream(const GuestStream&) = delete;
    GuestStream& operator=(const GuestStream&) = delete;
    GuestStream(GuestStream&&) = delete;
    GuestStream& operator=(GuestStream&&) = delete;

    /// The player's pad, keys and pointer as they stand now; sent when they changed. Any thread.
    void pad(const wire::PadState& state);
    void keys(const wire::KeyState& state);
    void mouse(const wire::MouseState& state);
    /// The mapping the pad is played with.
    void mapping(const wire::PadMapping& mapping);
    /// Takes at most `bitsPerSecond` from the host from now on, 0 for all: a test run's slow link.
    void capLink(std::uint64_t bitsPerSecond) {
        cap_.set(bitsPerSecond);
    }

    /// Ends the session and tells the host `reason`. Any thread.
    void end(std::string_view reason);
    /// Why the session is over; nothing while it runs. Any thread.
    [[nodiscard]] std::optional<std::string> over() const;

    /// The round trip and the host's clock as of the fastest recent ping.
    struct Timing {
        /// Whether a pong came yet.
        bool known{false};
        std::uint64_t roundTripUs{0};
        /// The host's clock minus this one's, in microseconds.
        std::int64_t hostAheadUs{0};
        /// Bytes received in the last whole second.
        std::uint64_t bytesPerSecond{0};
    };
    [[nodiscard]] Timing timing() const;

  private:
    void receive(const std::stop_token& stop);
    void ping(const std::stop_token& stop);
    void handle(const Opened& opened, std::uint64_t receivedUs);
    void send(PacketKind kind, std::string_view payload);
    void finish(std::string reason);
    /// Sends the input states held now.
    void sendInput();
    /// Sends what was received.
    void sendReport();

    Setup setup_;
    UdpSocket socket_;
    PacketOpener opener_;
    std::mutex sendMutex_;
    PacketSeal seal_;
    mutable std::mutex mutex_;
    std::condition_variable_any changed_;
    bool inputChanged_{false};
    std::optional<wire::PadState> pad_;
    std::optional<wire::KeyState> keys_;
    std::optional<wire::MouseState> mouse_;
    std::optional<wire::PadMapping> mapping_;
    /// What came from the host, for the report; on the receiving thread, read under `mutex_`.
    wire::Report received_;
    Timing timing_;
    LinkCap cap_;
    /// When a key frame was last asked for, on the receiving thread.
    std::uint64_t keyAskedUs_{0};
    std::optional<std::string> over_;
    /// Last, so they stop before what they use goes.
    std::jthread receiver_;
    std::jthread pinger_;
};

} // namespace opensu::stream
