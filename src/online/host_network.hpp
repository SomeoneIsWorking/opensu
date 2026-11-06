// host_network — what the system knows of its own network, which a code and a router mapping need:
// a name resolved to an address, this machine's interfaces, and its default gateway. IPv4, like
// `stream::UdpSocket`. Each system has its own file; Android has none, like its socket.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "stream/udp_socket.hpp"

namespace opensu::online {

/// An address of an up, non-loopback interface, both in network byte order.
struct Interface {
    std::uint32_t ip{0};
    std::uint32_t mask{0};
};

/// `host`, a name or a dotted address, with `port`; nothing when it does not resolve.
[[nodiscard]] std::optional<stream::UdpAddress> resolve(const std::string& host,
                                                        std::uint16_t port);

[[nodiscard]] std::vector<Interface> interfaces();

/// The default route's gateway, in network byte order.
[[nodiscard]] std::optional<std::uint32_t> defaultGateway();

/// `interfaces` in the order a code should keep them: Tailscale's first, as it is the fallback,
/// then the one `gateway` is on; link-local addresses left out.
[[nodiscard]] std::vector<std::uint32_t> reachableLocals(const std::vector<Interface>& interfaces,
                                                         std::optional<std::uint32_t> gateway);

/// Whether `ip`, in network byte order, can be reached only from inside some network: RFC 1918,
/// shared (CGNAT and Tailscale), link-local or loopback.
[[nodiscard]] bool privateAddress(std::uint32_t ip) noexcept;

} // namespace opensu::online
