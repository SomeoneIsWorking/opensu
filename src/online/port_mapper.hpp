// port_mapper — asking the router to forward a UDP port here, by whichever protocol it speaks
// (`NatPmp`, `UpnpIgd`). Always with a lease: a router offering only permanent mappings gets
// nothing from us, as a machine that dies would leave its port open.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "stream/udp_socket.hpp"

namespace opensu::online {

/// The most lease a mapping may have; the router's word for more is refused.
inline constexpr std::chrono::seconds longestLease{600};

/// A port the router forwards here.
struct Mapping {
    /// Where the internet reaches it.
    stream::UdpAddress external;
    std::chrono::seconds lease{0};
};

class PortMapper {
  public:
    PortMapper() = default;
    PortMapper(const PortMapper&) = delete;
    PortMapper& operator=(const PortMapper&) = delete;
    PortMapper(PortMapper&&) = delete;
    PortMapper& operator=(PortMapper&&) = delete;
    virtual ~PortMapper() = default;

    /// Forwards a public port to local `port` for `lease`, or renews it; nothing when the router
    /// will not. What was granted is a mapping with an address on the internet or nothing.
    [[nodiscard]] virtual std::optional<Mapping> open(std::uint16_t port,
                                                      std::chrono::seconds lease) = 0;
    /// Takes the mapping of local `port` down.
    virtual void close(std::uint16_t port) = 0;
};

} // namespace opensu::online
