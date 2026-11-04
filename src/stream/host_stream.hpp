// host_stream — the host's end of one remote-play session: its UDP socket, the seal for what it
// sends and the opener for what the guest sends. The guest is whoever first sends a datagram that
// opens; from then on only its address is heard. Its pings are answered, and the session is over
// when it says so, when it goes quiet, or when the host ends it (`end`, told to the guest).
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "lan/pairing_keys.hpp"
#include "packet_seal.hpp"
#include "stream_wire.hpp"
#include "udp_socket.hpp"

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
    /// Ends the session and tells the guest `reason`. Any thread.
    void end(const std::string& reason);
    /// Why the session is over; nothing while it runs. Any thread.
    [[nodiscard]] std::optional<std::string> over() const;
    /// Whether the guest has been heard.
    [[nodiscard]] bool joined() const noexcept {
        return joined_.load();
    }

  private:
    void receive(const std::stop_token& stop);
    void handle(const Opened& opened);
    void finish(std::string reason);

    Setup setup_;
    UdpSocket socket_;
    PacketOpener opener_;
    std::mutex sendMutex_;
    PacketSeal seal_;
    std::optional<UdpAddress> guest_;
    std::atomic<bool> joined_{false};
    mutable std::mutex overMutex_;
    std::optional<std::string> over_;
    /// Last, so it stops before what it uses goes.
    std::jthread receiver_;
};

} // namespace opensu::stream
