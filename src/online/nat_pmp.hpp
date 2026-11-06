// nat_pmp — a port mapping by NAT-PMP (RFC 6886): one small datagram to the gateway and its answer.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "port_mapper.hpp"

namespace opensu::online {

/// The gateway's NAT-PMP port.
inline constexpr std::uint16_t natPmpPort = 5351;

class NatPmp final : public PortMapper {
  public:
    /// Asks `gateway`, which only it can answer. Throws `std::system_error` with no socket to be
    /// had.
    explicit NatPmp(stream::UdpAddress gateway);

    [[nodiscard]] std::optional<Mapping> open(std::uint16_t port,
                                              std::chrono::seconds lease) override;
    void close(std::uint16_t port) override;

  private:
    /// The gateway's answer to `request`, of `answerBytes` with a good result code.
    [[nodiscard]] std::optional<std::string> ask(std::string_view request, std::size_t answerBytes);

    stream::UdpAddress gateway_;
    stream::UdpSocket socket_;
    std::uint16_t external_{0};
};

} // namespace opensu::online
