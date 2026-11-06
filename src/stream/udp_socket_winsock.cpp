#include "udp_socket.hpp"

#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <mswsock.h>

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

std::system_error socketError(const std::string& what) {
    return std::system_error{WSAGetLastError(), std::system_category(), what};
}

} // namespace

/// A Winsock socket, and the Winsock use it counts (WSAStartup counts each one).
std::string udpMissing() {
    return {};
}

class UdpSocket::Descriptor {
  public:
    Descriptor() {
        WSADATA data{};
        if (const int failed = WSAStartup(MAKEWORD(2, 2), &data); failed != 0) {
            throw std::system_error{failed, std::system_category(), "Winsock cannot start"};
        }
        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    ~Descriptor() {
        if (socket != INVALID_SOCKET) {
            ::closesocket(socket);
        }
        WSACleanup();
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    SOCKET socket{INVALID_SOCKET};
};

std::optional<UdpAddress> udpAddress(const std::string& dotted, std::uint16_t port) {
    in_addr parsed{};
    if (inet_pton(AF_INET, dotted.c_str(), &parsed) != 1) {
        return std::nullopt;
    }
    return UdpAddress{parsed.s_addr, port};
}

UdpSocket::UdpSocket(std::uint16_t port) : descriptor_{std::make_unique<Descriptor>()} {
    const SOCKET socket = descriptor_->socket;
    if (socket == INVALID_SOCKET) {
        throw socketError("cannot open a UDP socket");
    }
    // A datagram the guest no longer takes would otherwise end the next receive with
    // WSAECONNRESET.
    BOOL report = FALSE;
    DWORD returned = 0;
    ::WSAIoctl(socket, SIO_UDP_CONNRESET, &report, sizeof report, nullptr, 0, &returned, nullptr,
               nullptr);
    ::setsockopt(socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferBytes),
                 sizeof bufferBytes);
    ::setsockopt(socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferBytes),
                 sizeof bufferBytes);
    const sockaddr_in any = socketAddress(UdpAddress{htonl(INADDR_ANY), port});
    sockaddr_in bound{};
    int length = sizeof bound;
    if (::bind(socket, reinterpret_cast<const sockaddr*>(&any), sizeof any) != 0 ||
        ::getsockname(socket, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        throw socketError("cannot bind UDP port " + std::to_string(port));
    }
    port_ = ntohs(bound.sin_port);
}

UdpSocket::~UdpSocket() = default;

bool UdpSocket::send(const UdpAddress& to, std::string_view bytes) const noexcept {
    const sockaddr_in raw = socketAddress(to);
    return ::sendto(descriptor_->socket, bytes.data(), static_cast<int>(bytes.size()), 0,
                    reinterpret_cast<const sockaddr*>(&raw),
                    sizeof raw) == static_cast<int>(bytes.size());
}

bool UdpSocket::sendWithin(const UdpAddress& to, std::string_view bytes, int hops) const noexcept {
    const SOCKET socket = descriptor_->socket;
    int usual = 0;
    int size = sizeof usual;
    if (::getsockopt(socket, IPPROTO_IP, IP_TTL, reinterpret_cast<char*>(&usual), &size) != 0 ||
        ::setsockopt(socket, IPPROTO_IP, IP_TTL, reinterpret_cast<const char*>(&hops),
                     sizeof hops) != 0) {
        return false;
    }
    const bool sent = send(to, bytes);
    return ::setsockopt(socket, IPPROTO_IP, IP_TTL, reinterpret_cast<const char*>(&usual),
                        sizeof usual) == 0 &&
           sent;
}

std::optional<std::size_t> UdpSocket::receive(std::span<char> buffer, UdpAddress& from,
                                              std::chrono::milliseconds wait) const {
    const SOCKET socket = descriptor_->socket;
    WSAPOLLFD watched{.fd = socket, .events = POLLRDNORM, .revents = 0};
    if (::WSAPoll(&watched, 1, static_cast<INT>(wait.count())) <= 0) {
        return std::nullopt;
    }
    sockaddr_in sender{};
    int length = sizeof sender;
    const int got = ::recvfrom(socket, buffer.data(), static_cast<int>(buffer.size()), 0,
                               reinterpret_cast<sockaddr*>(&sender), &length);
    if (got < 0) {
        return std::nullopt;
    }
    from = UdpAddress{sender.sin_addr.s_addr, ntohs(sender.sin_port)};
    return static_cast<std::size_t>(got);
}

} // namespace opensu::stream
