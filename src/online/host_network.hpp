// host_network — what the system knows of its own network, which a code and a router mapping need:
// a name resolved to an address, which of this machine's interfaces a code keeps, and its default
// gateway. IPv4, like `stream::UdpSocket`. The interfaces are Lucent's (`lucent::net`); the lookups
// have a file per system, and Android has none, like its socket.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "lucent/network_interfaces.h"
#include "stream/udp_socket.hpp"

namespace opensu::online {

/// `host`, a name or a dotted address, with `port`; nothing when it does not resolve.
[[nodiscard]] std::optional<stream::UdpAddress> resolve(const std::string& host,
                                                        std::uint16_t port);

/// The default route's gateway, in network byte order.
[[nodiscard]] std::optional<std::uint32_t> defaultGateway();

/// `interface`'s address in network byte order, as `stream::UdpAddress` keeps it.
[[nodiscard]] std::uint32_t addressOf(const lucent::net::Ipv4Interface& interface) noexcept;

/// Whether `gateway`, in network byte order, is on `interface`'s network.
[[nodiscard]] bool onLinkWith(const lucent::net::Ipv4Interface& interface,
                              std::optional<std::uint32_t> gateway) noexcept;

/// Whether `interface` belongs to ZeroTier, by the name ZeroTier gives it or its driver's words.
/// A renamed one is not recognised and is ranked as any other interface.
[[nodiscard]] bool zeroTier(const lucent::net::Ipv4Interface& interface) noexcept;

/// The addresses of `interfaces` a code should keep, in network byte order and in the order to keep
/// them: Tailscale's and ZeroTier's first, as they are the fallback, then the one `gateway` is on.
/// Loopback, interfaces down or without a carrier, and link-local addresses are left out.
[[nodiscard]] std::vector<std::uint32_t>
reachableLocals(const std::vector<lucent::net::Ipv4Interface>& interfaces,
                std::optional<std::uint32_t> gateway);

/// Whether `ip`, in network byte order, can be reached only from inside some network: RFC 1918,
/// shared (CGNAT and Tailscale), link-local or loopback.
[[nodiscard]] bool privateAddress(std::uint32_t ip) noexcept;

} // namespace opensu::online
