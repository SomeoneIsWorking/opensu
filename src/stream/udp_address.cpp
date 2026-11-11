#include <array>
#include <cstring>
#include <format>

#include "udp_socket.hpp"

namespace opensu::stream {

std::string dotted(std::uint32_t ip) {
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &ip, bytes.size());
    return std::format("{}.{}.{}.{}", bytes[0], bytes[1], bytes[2], bytes[3]);
}

} // namespace opensu::stream
