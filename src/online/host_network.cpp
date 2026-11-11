#include "host_network.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <string_view>

namespace opensu::online {
namespace {

using lucent::net::Ipv4Interface;

std::array<std::uint8_t, 4> octets(std::uint32_t ip) noexcept {
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &ip, bytes.size());
    return bytes;
}

bool linkLocal(std::uint32_t ip) noexcept {
    const auto bytes = octets(ip);
    return bytes[0] == 169 && bytes[1] == 254;
}

/// 100.64.0.0/10, which Tailscale takes its addresses from.
bool shared(std::uint32_t ip) noexcept {
    const auto bytes = octets(ip);
    return bytes[0] == 100 && (bytes[1] & 0xC0U) == 64;
}

/// `hostOrder` in network byte order.
std::uint32_t networkOrder(std::uint32_t hostOrder) noexcept {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(hostOrder >> 24U), static_cast<std::uint8_t>(hostOrder >> 16U),
        static_cast<std::uint8_t>(hostOrder >> 8U), static_cast<std::uint8_t>(hostOrder)};
    std::uint32_t ip = 0;
    std::memcpy(&ip, bytes.data(), sizeof ip);
    return ip;
}

/// ZeroTier's names: `zt` and a hash of the network on Linux, `feth` pairs on macOS.
constexpr std::array<std::string_view, 2> zeroTierNames{"zt", "feth"};
/// Windows' adapter description, "ZeroTier Virtual Port".
constexpr std::string_view zeroTierDriver = "ZeroTier";

} // namespace

std::uint32_t addressOf(const Ipv4Interface& interface) noexcept {
    return networkOrder(interface.address);
}

bool onLinkWith(const Ipv4Interface& interface, std::optional<std::uint32_t> gateway) noexcept {
    const std::uint32_t mask = networkOrder(interface.netmask);
    return gateway && (*gateway & mask) == (addressOf(interface) & mask);
}

bool zeroTier(const Ipv4Interface& interface) noexcept {
    return interface.description.find(zeroTierDriver) != std::string::npos ||
           std::ranges::any_of(zeroTierNames, [&interface](std::string_view prefix) {
               return interface.name.starts_with(prefix);
           });
}

std::vector<std::uint32_t> reachableLocals(const std::vector<Ipv4Interface>& interfaces,
                                           std::optional<std::uint32_t> gateway) {
    const auto rank = [gateway](const Ipv4Interface& interface) {
        if (shared(addressOf(interface)) || zeroTier(interface)) {
            return 0;
        }
        return onLinkWith(interface, gateway) ? 1 : 2;
    };
    std::vector<Ipv4Interface> kept;
    std::ranges::copy_if(interfaces, std::back_inserter(kept), [](const Ipv4Interface& interface) {
        return interface.up && interface.running && !interface.loopback &&
               !linkLocal(addressOf(interface));
    });
    std::ranges::stable_sort(kept, {}, rank);
    std::vector<std::uint32_t> ordered;
    ordered.reserve(kept.size());
    for (const Ipv4Interface& interface : kept) {
        ordered.push_back(addressOf(interface));
    }
    return ordered;
}

bool privateAddress(std::uint32_t ip) noexcept {
    const auto bytes = octets(ip);
    return bytes[0] == 10 || bytes[0] == 127 || (bytes[0] == 172 && (bytes[1] & 0xF0U) == 16) ||
           (bytes[0] == 192 && bytes[1] == 168) || linkLocal(ip) || shared(ip);
}

} // namespace opensu::online
