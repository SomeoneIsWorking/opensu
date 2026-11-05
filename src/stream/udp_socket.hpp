// udp_socket — remote play's one UDP socket per session, IPv4, bound on every interface: datagrams
// sent to an address and received with where they came from, waiting at most a given time so a
// thread reading it can stop.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace opensu::stream {

/// An IPv4 address and port.
struct UdpAddress {
    /// In network byte order.
    std::uint32_t ip{0};
    std::uint16_t port{0};

    bool operator==(const UdpAddress&) const = default;
};

/// `dotted` ("192.168.1.20") and `port`; nothing when `dotted` is not an IPv4 address.
[[nodiscard]] std::optional<UdpAddress> udpAddress(const std::string& dotted, std::uint16_t port);

/// Why this system has no socket for a session; empty where it has.
[[nodiscard]] std::string udpMissing();

/// The largest datagram a session takes.
inline constexpr std::size_t largestDatagram = 65536;

class UdpSocket {
  public:
    /// Binds `port` on every IPv4 interface, a free one when 0; throws `std::system_error`.
    explicit UdpSocket(std::uint16_t port);
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) = delete;
    UdpSocket& operator=(UdpSocket&&) = delete;

    [[nodiscard]] std::uint16_t port() const noexcept {
        return port_;
    }
    /// Sends `bytes` to `to`; false when the system refused it (a full buffer drops it).
    bool send(const UdpAddress& to, std::string_view bytes) const noexcept;
    /// The next datagram into `buffer` (`largestDatagram` bytes), waiting at most `wait`: its
    /// length, and its sender in `from`; nothing when none came.
    [[nodiscard]] std::optional<std::size_t> receive(std::span<char> buffer, UdpAddress& from,
                                                     std::chrono::milliseconds wait) const;

  private:
    /// The system's socket, which each backend defines.
    class Descriptor;
    std::unique_ptr<Descriptor> descriptor_;
    std::uint16_t port_{0};
};

} // namespace opensu::stream
