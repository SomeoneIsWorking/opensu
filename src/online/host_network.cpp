#include "host_network.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

namespace opensu::online {
namespace {

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

} // namespace

std::vector<std::uint32_t> reachableLocals(const std::vector<Interface>& interfaces,
                                           std::optional<std::uint32_t> gateway) {
    const auto rank = [gateway](const Interface& interface) {
        if (shared(interface.ip)) {
            return 0;
        }
        return gateway && (*gateway & interface.mask) == (interface.ip & interface.mask) ? 1 : 2;
    };
    std::vector<Interface> kept;
    std::ranges::copy_if(interfaces, std::back_inserter(kept), [](const Interface& interface) {
        return !linkLocal(interface.ip);
    });
    std::ranges::stable_sort(kept, {}, rank);
    std::vector<std::uint32_t> ordered;
    ordered.reserve(kept.size());
    for (const Interface& interface : kept) {
        ordered.push_back(interface.ip);
    }
    return ordered;
}

bool privateAddress(std::uint32_t ip) noexcept {
    const auto bytes = octets(ip);
    return bytes[0] == 10 || bytes[0] == 127 || (bytes[0] == 172 && (bytes[1] & 0xF0U) == 16) ||
           (bytes[0] == 192 && bytes[1] == 168) || linkLocal(ip) || shared(ip);
}

} // namespace opensu::online
