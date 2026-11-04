#include "udp_socket.hpp"

#include <cerrno>
#include <system_error>

namespace opensu::stream {

std::optional<UdpAddress> udpAddress(const std::string& /*dotted*/, std::uint16_t /*port*/) {
    return std::nullopt;
}

class UdpSocket::Descriptor {};

UdpSocket::UdpSocket(std::uint16_t /*port*/) {
    throw std::system_error{ENOSYS, std::generic_category(),
                            "remote play is not built for this system yet"};
}

UdpSocket::~UdpSocket() = default;

bool UdpSocket::send(const UdpAddress& /*to*/, std::string_view /*bytes*/) const noexcept {
    return false;
}

std::optional<std::size_t> UdpSocket::receive(std::span<char> /*buffer*/, UdpAddress& /*from*/,
                                              std::chrono::milliseconds /*wait*/) const {
    return std::nullopt;
}

} // namespace opensu::stream
