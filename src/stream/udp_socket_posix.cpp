#include "udp_socket.hpp"

#include <array>
#include <cerrno>
#include <memory>
#include <system_error>

#include <arpa/inet.h>
#include <fcntl.h>
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

class UdpSocket::Descriptor {
  public:
    Descriptor() noexcept : fd{opened()} {
    }
    ~Descriptor() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    const int fd;

  private:
    /// A UDP socket closed on exec; macOS has no SOCK_CLOEXEC, so it is set right after.
    static int opened() noexcept {
#ifdef SOCK_CLOEXEC
        return ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
#else
        const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd >= 0) {
            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
        return fd;
#endif
    }
};

std::optional<UdpAddress> udpAddress(const std::string& dotted, std::uint16_t port) {
    in_addr parsed{};
    if (inet_pton(AF_INET, dotted.c_str(), &parsed) != 1) {
        return std::nullopt;
    }
    return UdpAddress{parsed.s_addr, port};
}

UdpSocket::UdpSocket(std::uint16_t port) : descriptor_{std::make_unique<Descriptor>()} {
    const int fd = descriptor_->fd;
    if (fd < 0) {
        throw std::system_error{errno, std::generic_category(), "cannot open a UDP socket"};
    }
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufferBytes, sizeof bufferBytes);
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufferBytes, sizeof bufferBytes);
    const sockaddr_in any = socketAddress(UdpAddress{htonl(INADDR_ANY), port});
    sockaddr_in bound{};
    socklen_t length = sizeof bound;
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&any), sizeof any) != 0 ||
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        throw std::system_error{errno, std::generic_category(),
                                "cannot bind UDP port " + std::to_string(port)};
    }
    port_ = ntohs(bound.sin_port);
}

UdpSocket::~UdpSocket() = default;

bool UdpSocket::send(const UdpAddress& to, std::string_view bytes) const noexcept {
    const sockaddr_in raw = socketAddress(to);
    // A datagram socket never raises SIGPIPE.
    return ::sendto(descriptor_->fd, bytes.data(), bytes.size(), 0,
                    reinterpret_cast<const sockaddr*>(&raw),
                    sizeof raw) == static_cast<ssize_t>(bytes.size());
}

bool UdpSocket::sendWithin(const UdpAddress& to, std::string_view bytes, int hops) const noexcept {
    const int fd = descriptor_->fd;
    int usual = 0;
    socklen_t size = sizeof usual;
    if (::getsockopt(fd, IPPROTO_IP, IP_TTL, &usual, &size) != 0 ||
        ::setsockopt(fd, IPPROTO_IP, IP_TTL, &hops, sizeof hops) != 0) {
        return false;
    }
    const bool sent = send(to, bytes);
    return ::setsockopt(fd, IPPROTO_IP, IP_TTL, &usual, sizeof usual) == 0 && sent;
}

std::optional<std::size_t> UdpSocket::receive(std::span<char> buffer, UdpAddress& from,
                                              std::chrono::milliseconds wait) const {
    sockaddr_in sender{};
    socklen_t length = sizeof sender;
    const int fd = descriptor_->fd;
    ssize_t got = ::recvfrom(fd, buffer.data(), buffer.size(), MSG_DONTWAIT,
                             reinterpret_cast<sockaddr*>(&sender), &length);
    if (got < 0) {
        pollfd watched{.fd = fd, .events = POLLIN, .revents = 0};
        if (::poll(&watched, 1, static_cast<int>(wait.count())) <= 0) {
            return std::nullopt;
        }
        length = sizeof sender;
        got = ::recvfrom(fd, buffer.data(), buffer.size(), MSG_DONTWAIT,
                         reinterpret_cast<sockaddr*>(&sender), &length);
    }
    if (got < 0) {
        return std::nullopt;
    }
    from = UdpAddress{sender.sin_addr.s_addr, ntohs(sender.sin_port)};
    return static_cast<std::size_t>(got);
}

} // namespace opensu::stream
