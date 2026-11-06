#include <array>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#ifdef __APPLE__
#include <net/route.h>
#include <sys/sysctl.h>
#include <vector>
#endif

#include "host_network.hpp"

namespace opensu::online {
namespace {

struct FreeAddresses {
    void operator()(addrinfo* list) const noexcept {
        freeaddrinfo(list);
    }
};

struct FreeInterfaces {
    void operator()(ifaddrs* list) const noexcept {
        freeifaddrs(list);
    }
};

std::uint32_t ipOf(const sockaddr* address) noexcept {
    std::uint32_t ip = 0;
    std::memcpy(&ip, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, sizeof ip);
    return ip;
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
    const std::unique_ptr<addrinfo, FreeAddresses> list{found};
    return stream::UdpAddress{.ip = ipOf(list->ai_addr), .port = port};
}

std::vector<Interface> interfaces() {
    ifaddrs* found = nullptr;
    if (getifaddrs(&found) != 0) {
        return {};
    }
    const std::unique_ptr<ifaddrs, FreeInterfaces> list{found};
    std::vector<Interface> all;
    for (const ifaddrs* at = list.get(); at != nullptr; at = at->ifa_next) {
        if (at->ifa_addr != nullptr && at->ifa_netmask != nullptr &&
            at->ifa_addr->sa_family == AF_INET && (at->ifa_flags & IFF_UP) != 0 &&
            (at->ifa_flags & IFF_LOOPBACK) == 0) {
            all.push_back({.ip = ipOf(at->ifa_addr), .mask = ipOf(at->ifa_netmask)});
        }
    }
    return all;
}

#ifdef __APPLE__

std::optional<std::uint32_t> defaultGateway() {
    std::array<int, 6> query{CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY};
    std::size_t size = 0;
    if (sysctl(query.data(), query.size(), nullptr, &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    std::vector<char> table(size);
    if (sysctl(query.data(), query.size(), table.data(), &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    for (std::size_t at = 0; at + sizeof(rt_msghdr) <= size;) {
        const auto* message = reinterpret_cast<const rt_msghdr*>(table.data() + at);
        const auto* address = reinterpret_cast<const sockaddr*>(message + 1);
        if ((message->rtm_addrs & (RTA_DST | RTA_GATEWAY)) == (RTA_DST | RTA_GATEWAY) &&
            address->sa_family == AF_INET && ipOf(address) == 0) {
            const std::size_t skip = (address->sa_len + 3U) & ~std::size_t{3};
            const auto* gateway =
                reinterpret_cast<const sockaddr*>(reinterpret_cast<const char*>(address) + skip);
            if (gateway->sa_family == AF_INET) {
                return ipOf(gateway);
            }
        }
        at += message->rtm_msglen;
    }
    return std::nullopt;
}

#else

std::optional<std::uint32_t> defaultGateway() {
    std::ifstream routes{"/proc/net/route"};
    std::string line;
    std::getline(routes, line);
    while (std::getline(routes, line)) {
        std::istringstream fields{line};
        std::string name;
        std::uint32_t destination = 0;
        std::uint32_t gateway = 0;
        // The kernel writes both as the bytes in memory, so in network order on any host.
        fields >> name >> std::hex >> destination >> gateway;
        if (fields && destination == 0 && gateway != 0) {
            return gateway;
        }
    }
    return std::nullopt;
}

#endif

} // namespace opensu::online
