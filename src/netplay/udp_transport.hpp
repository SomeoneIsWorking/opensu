// udp_transport — a netplay session's messages over UDP, on the LAN or the internet alike: one
// socket and one other player, each datagram sealed with the session's key for its sender
// (`stream::PacketSeal`). The peer is the address that last sent a datagram that opened, so a side
// told nothing learns it and a NAT rebinding a port costs nothing.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "stream/packet_seal.hpp"
#include "stream/udp_socket.hpp"
#include "transport.hpp"

namespace opensu::netplay {

class UdpTransport final : public Transport {
  public:
    /// Binds `port`, a free one when 0, and sends to `peer` once known. Throws
    /// `std::system_error` when the socket cannot be had.
    UdpTransport(std::uint16_t port, std::optional<stream::UdpAddress> peer,
                 std::span<const std::uint8_t> sendKey, std::span<const std::uint8_t> receiveKey);

    [[nodiscard]] std::uint16_t port() const noexcept {
        return socket_.port();
    }
    void send(std::string_view message) override;
    [[nodiscard]] std::optional<std::string> receive(std::chrono::milliseconds wait) override;

  private:
    stream::UdpSocket socket_;
    stream::PacketSeal seal_;
    stream::PacketOpener opener_;
    std::optional<stream::UdpAddress> peer_;
    std::vector<char> buffer_;
};

} // namespace opensu::netplay
