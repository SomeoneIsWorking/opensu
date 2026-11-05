// lan_transport — a netplay session's messages over the LAN: one UDP socket and one other player,
// each datagram sealed with the session's key for its sender (`stream::PacketSeal`, keys from the
// pairing through `lan::streamKeys`). The side that does not know the other's address learns it
// from the first datagram that opens.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "stream/packet_seal.hpp"
#include "stream/udp_socket.hpp"
#include "transport.hpp"

namespace opensu::netplay {

class LanTransport final : public Transport {
  public:
    /// Binds `port`, a free one when 0, and sends to `peer` once known. Throws
    /// `std::system_error` when the socket cannot be had.
    LanTransport(std::uint16_t port, std::optional<stream::UdpAddress> peer,
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
