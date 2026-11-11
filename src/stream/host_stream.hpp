// host_stream — the host's end of one remote-play session: its UDP socket, the seal for what it
// sends and the opener for what the guest sends. The guest is whoever first sends a datagram that
// opens; from then on only its address is heard. Its pings are answered, its reports measure the
// link (`LinkRate`) and its asks for a key frame are kept for the video. What the host sends of
// video and probes leaves through one pacer (`TokenBucket`) on a thread of its own: what one frame
// at the plan's rate holds leaves at once, and the rest follows at twice that rate, so a frame
// larger than the plan's (a key frame) does not reach a shallow link queue as one burst. While
// the link probes, the pacer fills at the probe's rate instead and probe datagrams take what the
// video leaves of it, always a frame's worth kept back for the next. The session is over when the
// guest says so, when it goes quiet, or when the host ends it (`end`, told to the guest).
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "lan/pairing_keys.hpp"
#include "link_rate.hpp"
#include "packet_seal.hpp"
#include "stream_wire.hpp"
#include "token_bucket.hpp"
#include "udp_socket.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

class HostStream {
  public:
    struct Setup {
        lan::StreamKeys keys;
        /// The guest's name, for the log.
        std::string guest;
        /// The guest's pad, keys and pointer as they stand; called on the stream's thread.
        std::function<void(const wire::PadState&)> pad;
        std::function<void(const wire::KeyState&)> keyboard;
        std::function<void(const wire::MouseState&)> mouse;
        /// The guest's mapping for its pad, whenever it is heard; on the stream's thread.
        std::function<void(const wire::PadMapping&)> mapping;
        /// The codecs this host codes.
        VideoCodecs encoders{0};
    };

    /// Binds a free UDP port; throws `std::system_error` when none can be had.
    explicit HostStream(Setup setup);
    ~HostStream();
    HostStream(const HostStream&) = delete;
    HostStream& operator=(const HostStream&) = delete;
    HostStream(HostStream&&) = delete;
    HostStream& operator=(HostStream&&) = delete;

    [[nodiscard]] std::uint16_t port() const noexcept {
        return socket_.port();
    }
    /// Sends a datagram of `kind` to the guest once it is known; dropped before. Any thread.
    void send(PacketKind kind, std::string_view payload);
    /// Sends one video frame's payloads, paced behind any of the last frame's still waiting. Any
    /// thread.
    void sendFrame(std::vector<std::string> payloads);
    /// Ends the session and tells the guest `reason`. Any thread.
    void end(std::string_view reason);
    /// Why the session is over; nothing while it runs. Any thread.
    [[nodiscard]] std::optional<std::string> over() const;
    /// Whether it is over because a guest that had joined went quiet.
    [[nodiscard]] bool dropped() const noexcept {
        return dropped_;
    }
    /// Whether the guest has been heard.
    [[nodiscard]] bool joined() const noexcept {
        return joined_.load();
    }

    /// What to code at now; nothing until the link was probed. Any thread.
    [[nodiscard]] std::optional<LinkPlan> plan() const;
    /// The pixels the guest shows the picture in; empty until it says. Any thread.
    [[nodiscard]] PictureSize shownIn() const;
    /// The frames a second the guest wants, 0 for every one captured. Any thread.
    [[nodiscard]] std::uint8_t wantedRate() const noexcept {
        return wantedRate_.load();
    }
    /// `codec` cannot code here; the link plans another. Any thread.
    void refuse(VideoCodec codec);
    /// Whether the guest asked for a key frame since the last call. Any thread.
    [[nodiscard]] bool takeKeyFrame() noexcept {
        return keyFrame_.exchange(false);
    }
    /// The link's plan and measures (`LinkRate::summary`), and the video datagrams the pacer held
    /// back of those sent (`paced=`). Any thread.
    [[nodiscard]] std::string linkSummary() const;

  private:
    void receive(const std::stop_token& stop);
    /// Sends the paced video and the probe datagrams as they fall due.
    void pace(const std::stop_token& stop);
    /// Sets the pacer's rate for the probe under way or the plan. Holding `paceMutex_`.
    void tune(std::uint64_t nowUs);
    /// Sends the waiting video the pacer allows at `nowUs`. Holding `paceMutex_`.
    void sendDue(std::uint64_t nowUs);
    /// Sends the probe datagrams the pacer allows at `nowUs` with no video waiting and a frame's
    /// worth kept; how many. Holding `paceMutex_`.
    std::uint32_t pad(std::uint64_t nowUs);
    void handle(const Opened& opened);
    void finish(std::string reason);

    Setup setup_;
    UdpSocket socket_;
    PacketOpener opener_;
    std::mutex sendMutex_;
    PacketSeal seal_;
    std::optional<UdpAddress> guest_;
    std::atomic<bool> joined_{false};
    /// Over because a guest that had joined went quiet.
    std::atomic<bool> dropped_{false};
    mutable std::mutex overMutex_;
    std::optional<std::string> over_;
    mutable std::mutex linkMutex_;
    LinkRate link_;
    PictureSize shownIn_;
    std::atomic<std::uint8_t> wantedRate_{0};
    std::atomic<bool> keyFrame_{false};
    /// The pacer, the rate it fills at, the plan's rate, the frame's worth it keeps for video, the
    /// probe under way, the frames' payloads waiting on it, and the video datagrams sent and how
    /// many of them waited.
    mutable std::mutex paceMutex_;
    std::condition_variable_any paceWake_;
    TokenBucket pacer_;
    double pacerBytesPerSecond_{0};
    std::uint64_t pacedRate_{0};
    std::size_t reserve_{probeDatagramBytes};
    std::optional<LinkRate::Probing> probing_;
    std::deque<std::string> waiting_;
    bool queued_{false};
    std::uint64_t videoSent_{0};
    std::uint64_t paced_{0};
    /// Last, so they stop before what they use goes.
    std::jthread receiver_;
    std::jthread sender_;
};

} // namespace opensu::stream
