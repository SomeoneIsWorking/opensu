// upnp_igd — a port mapping through a router's UPnP Internet Gateway Device: found by SSDP, its
// description read for the WAN connection's control URL, then AddPortMapping and DeletePortMapping
// by SOAP. What a device on the LAN says is not trusted: its URLs must be on the address that
// answered the search, bodies are capped, and nothing is followed elsewhere.
#pragma once

#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#include "net/web_client.hpp"
#include "port_mapper.hpp"

namespace opensu::online {

/// Where SSDP searches go.
inline constexpr std::uint16_t ssdpPort = 1900;

/// The control URL and service type a description's WAN connection has, with its URL resolved
/// against `base` (`http://host:port`). Nothing when it lists none or names another host.
struct IgdService {
    std::string controlUrl;
    std::string serviceType;
};
[[nodiscard]] std::optional<IgdService> igdServiceIn(std::string_view description,
                                                     std::string_view base);

/// The value of the LOCATION header of an SSDP answer.
[[nodiscard]] std::optional<std::string> ssdpLocation(std::string_view answer);

class UpnpIgd final : public PortMapper {
  public:
    /// Searches `ssdp` for a router for at most `wait` and reads its description; nothing when none
    /// answers. `localIp` is where its mappings point, in network byte order.
    [[nodiscard]] static std::unique_ptr<UpnpIgd> discover(const std::stop_token& stop,
                                                           stream::UdpAddress ssdp,
                                                           std::uint32_t localIp,
                                                           std::chrono::milliseconds wait);

    [[nodiscard]] std::optional<Mapping> open(std::uint16_t port,
                                              std::chrono::seconds lease) override;
    void close(std::uint16_t port) override;

  private:
    UpnpIgd(std::string origin, IgdService service, std::string localIp);

    /// The answer to SOAP `action` with `arguments`: its body when it succeeded. A failure's UPnP
    /// error code goes in `error`.
    [[nodiscard]] std::optional<std::string> call(std::string_view action,
                                                  const std::string& arguments, int& error) const;

    net::WebClient web_;
    std::string origin_;
    IgdService service_;
    std::string localIp_;
    std::uint16_t external_{0};
};

} // namespace opensu::online
