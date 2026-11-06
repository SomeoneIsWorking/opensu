#include "udp_transport.hpp"

namespace opensu::netplay {

UdpTransport::UdpTransport(std::uint16_t port, std::optional<stream::UdpAddress> peer,
                           std::span<const std::uint8_t> sendKey,
                           std::span<const std::uint8_t> receiveKey)
    : socket_{port}, seal_{sendKey}, opener_{receiveKey}, peer_{peer},
      buffer_(stream::largestDatagram) {
}

void UdpTransport::send(std::string_view message) {
    if (peer_) {
        static_cast<void>(socket_.send(*peer_, seal_.seal(stream::PacketKind::Netplay, message)));
    }
}

std::optional<std::string> UdpTransport::receive(std::chrono::milliseconds wait) {
    stream::UdpAddress from;
    const std::optional<std::size_t> length = socket_.receive(buffer_, from, wait);
    if (!length) {
        return std::nullopt;
    }
    std::optional<stream::Opened> opened = opener_.open({buffer_.data(), *length});
    if (!opened || opened->kind != stream::PacketKind::Netplay) {
        return std::nullopt;
    }
    peer_ = from;
    return std::move(opened->payload);
}

} // namespace opensu::netplay
