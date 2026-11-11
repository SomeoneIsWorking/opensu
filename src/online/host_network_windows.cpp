#include "host_network.hpp"

#include <cstring>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>

namespace opensu::online {
namespace {

constexpr ULONG adapterFlags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST |
                               GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
/// Two tries: the list can grow between asking its size and reading it.
constexpr int adapterTries = 2;

std::uint32_t ipOf(const sockaddr* address) noexcept {
    std::uint32_t ip = 0;
    std::memcpy(&ip, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, sizeof ip);
    return ip;
}

/// The adapters' list, in a buffer that holds it; empty when the system gives none.
std::unique_ptr<char[]> adapters() {
    ULONG size = 16 * 1024;
    for (int tries = 0; tries < adapterTries; ++tries) {
        auto list = std::make_unique<char[]>(size);
        const ULONG result =
            GetAdaptersAddresses(AF_INET, adapterFlags, nullptr,
                                 reinterpret_cast<IP_ADAPTER_ADDRESSES*>(list.get()), &size);
        if (result == ERROR_SUCCESS) {
            return list;
        }
        if (result != ERROR_BUFFER_OVERFLOW) {
            break;
        }
    }
    return nullptr;
}

} // namespace

std::optional<stream::UdpAddress> resolve(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0) {
        return std::nullopt;
    }
    const stream::UdpAddress address{.ip = ipOf(found->ai_addr), .port = port};
    freeaddrinfo(found);
    return address;
}

std::optional<std::uint32_t> defaultGateway() {
    const std::unique_ptr<char[]> list = adapters();
    for (const auto* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(list.get());
         adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus == IfOperStatusUp && adapter->FirstGatewayAddress != nullptr) {
            return ipOf(adapter->FirstGatewayAddress->Address.lpSockaddr);
        }
    }
    return std::nullopt;
}

} // namespace opensu::online
