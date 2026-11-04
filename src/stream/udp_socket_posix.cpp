#include "udp_socket.hpp"

#include <array>
#include <cerrno>
#include <system_error>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace opensu::stream {
namespace {

/// Room for a frame's packets while the reader catches up; the system may cap it lower.
constexpr int bufferBytes = 8 * 1024 * 1024;

sockaddr_in socketAddress(const UdpAddress& address) {
    sockaddr_in raw{};
    raw.sin_family = AF_INET;
    raw.sin_addr.s_addr = address.ip;
    raw.sin_port = htons(address.port);
    return raw;
}

} // namespace

std::optional<UdpAddress> udpAddress(const std::string& dotted, std::uint16_t port) {
    in_addr parsed{};
    if (inet_pton(AF_INET, dotted.c_str(), &parsed) != 1) {
        return std::nullopt;
    }
    return UdpAddress{parsed.s_addr, port};
}

UdpSocket::UdpSocket(std::uint16_t port) : fd_{::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)} {
    if (fd_ < 0) {
        throw std::system_error{errno, std::generic_category(), "cannot open a UDP socket"};
    }
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bufferBytes, sizeof bufferBytes);
    ::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &bufferBytes, sizeof bufferBytes);
    const sockaddr_in any = socketAddress(UdpAddress{htonl(INADDR_ANY), port});
    sockaddr_in bound{};
    socklen_t length = sizeof bound;
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&any), sizeof any) != 0 ||
        ::getsockname(fd_, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        const int error = errno;
        ::close(fd_);
        throw std::system_error{error, std::generic_category(),
                                "cannot bind UDP port " + std::to_string(port)};
    }
    port_ = ntohs(bound.sin_port);
}

UdpSocket::~UdpSocket() {
    ::close(fd_);
}

bool UdpSocket::send(const UdpAddress& to, std::string_view bytes) const noexcept {
    const sockaddr_in raw = socketAddress(to);
    return ::sendto(fd_, bytes.data(), bytes.size(), MSG_NOSIGNAL,
                    reinterpret_cast<const sockaddr*>(&raw),
                    sizeof raw) == static_cast<ssize_t>(bytes.size());
}

std::optional<std::size_t> UdpSocket::receive(std::span<char> buffer, UdpAddress& from,
                                              std::chrono::milliseconds wait) const {
    sockaddr_in sender{};
    socklen_t length = sizeof sender;
    ssize_t got = ::recvfrom(fd_, buffer.data(), buffer.size(), MSG_DONTWAIT,
                             reinterpret_cast<sockaddr*>(&sender), &length);
    if (got < 0) {
        pollfd watched{.fd = fd_, .events = POLLIN, .revents = 0};
        if (::poll(&watched, 1, static_cast<int>(wait.count())) <= 0) {
            return std::nullopt;
        }
        length = sizeof sender;
        got = ::recvfrom(fd_, buffer.data(), buffer.size(), MSG_DONTWAIT,
                         reinterpret_cast<sockaddr*>(&sender), &length);
    }
    if (got < 0) {
        return std::nullopt;
    }
    from = UdpAddress{sender.sin_addr.s_addr, ntohs(sender.sin_port)};
    return static_cast<std::size_t>(got);
}

} // namespace opensu::stream
