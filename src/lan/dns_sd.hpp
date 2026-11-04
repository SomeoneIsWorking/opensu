// dns_sd — this openSU announced on the LAN as a `_opensu._tcp` DNS-SD service, and the other
// openSUs announced there, while it lives. One backend per OS: `dns_sd_avahi.cpp` on Linux, over
// Avahi's D-Bus API on a thread of its own; `dns_sd_none.cpp` elsewhere, which says it is not
// built yet. A service's TXT record carries its peer id (`id`), name (`name`) and protocol
// version (`v`); its own announcement is never listed.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::lan {

/// The DNS-SD service type every openSU announces.
inline constexpr std::string_view serviceType = "_opensu._tcp";
/// The protocol a service's `v` names.
inline constexpr std::string_view protocolVersion = "1";

/// What this openSU announces.
struct Announcement {
    std::string id;
    std::string name;
    std::uint16_t port{0};
};

/// Another openSU announced on the LAN, resolved to an IPv4 address.
struct Sighting {
    std::string id;
    std::string name;
    std::string address;
    std::uint16_t port{0};

    bool operator==(const Sighting&) const = default;
};

class ServiceDiscovery {
  public:
    virtual ~ServiceDiscovery() = default;

    /// The other openSUs announced now, one per peer id. Callable from any thread.
    [[nodiscard]] virtual std::vector<Sighting> sightings() const = 0;
    /// The name this one is announced under, which a clash on the LAN may have changed.
    [[nodiscard]] virtual std::string announcedName() const = 0;
};

/// The name this machine goes by on the LAN, which an openSU the player has not named takes.
[[nodiscard]] std::string machineName();

/// Why this build cannot announce or find openSUs, in words for the player; empty when it can.
[[nodiscard]] std::string_view discoveryMissing() noexcept;

/// Announces `self` and starts looking for the others; null with `error` when it cannot (no
/// system bus, no Avahi, or no backend in this build).
[[nodiscard]] std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& self,
                                                               std::string& error);

} // namespace opensu::lan
