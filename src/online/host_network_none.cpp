#include "host_network.hpp"

namespace opensu::online {

std::optional<stream::UdpAddress> resolve(const std::string& /*host*/, std::uint16_t /*port*/) {
    return std::nullopt;
}

std::vector<Interface> interfaces() {
    return {};
}

std::optional<std::uint32_t> defaultGateway() {
    return std::nullopt;
}

} // namespace opensu::online
