#include "stun.hpp"

#include <algorithm>
#include <cstring>

#include "secrets/crypto.hpp"

namespace opensu::online {
namespace {

constexpr std::size_t headerBytes = 20;
constexpr std::size_t idAt = 8;
constexpr std::size_t cookieAt = 4;
constexpr std::array<std::uint8_t, 4> cookie{0x21, 0x12, 0xA4, 0x42};
constexpr std::uint16_t bindingRequest = 0x0001;
constexpr std::uint16_t bindingSuccess = 0x0101;
constexpr std::uint16_t mappedAddress = 0x0001;
constexpr std::uint16_t xorMappedAddress = 0x0020;
constexpr std::uint8_t familyIpv4 = 0x01;
constexpr std::size_t addressBytes = 8;

std::uint16_t word(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(bytes[at]) << 8U) |
                                      static_cast<std::uint8_t>(bytes[at + 1]));
}

/// The address in attribute `value`, its port and IP masked with the cookie when `masked`.
std::optional<stream::UdpAddress> addressIn(std::string_view value, bool masked) {
    if (value.size() < addressBytes || static_cast<std::uint8_t>(value[1]) != familyIpv4) {
        return std::nullopt;
    }
    std::uint16_t port = word(value, 2);
    std::array<std::uint8_t, 4> ip{};
    std::memcpy(ip.data(), value.data() + 4, ip.size());
    if (masked) {
        port = static_cast<std::uint16_t>(port ^ ((cookie[0] << 8U) | cookie[1]));
        for (std::size_t at = 0; at < ip.size(); ++at) {
            ip[at] = static_cast<std::uint8_t>(ip[at] ^ cookie[at]);
        }
    }
    stream::UdpAddress address{.ip = 0, .port = port};
    std::memcpy(&address.ip, ip.data(), ip.size());
    return address;
}

} // namespace

StunId newStunId() {
    const secrets::crypto::Bytes random = secrets::crypto::randomBytes(stunIdBytes);
    StunId id{};
    std::ranges::copy(random, id.begin());
    return id;
}

std::string stunRequest(const StunId& id) {
    std::string message;
    message.reserve(headerBytes);
    message.push_back(static_cast<char>(bindingRequest >> 8U));
    message.push_back(static_cast<char>(bindingRequest & 0xFFU));
    message.append(2, '\0');
    message.append(reinterpret_cast<const char*>(cookie.data()), cookie.size());
    message.append(reinterpret_cast<const char*>(id.data()), id.size());
    return message;
}

bool isStun(std::string_view datagram) noexcept {
    return datagram.size() >= headerBytes &&
           (static_cast<std::uint8_t>(datagram[0]) & 0xC0U) == 0 &&
           std::memcmp(datagram.data() + cookieAt, cookie.data(), cookie.size()) == 0;
}

std::optional<stream::UdpAddress> stunMapped(std::string_view datagram, const StunId& id) {
    if (!isStun(datagram) || word(datagram, 0) != bindingSuccess ||
        std::memcmp(datagram.data() + idAt, id.data(), id.size()) != 0) {
        return std::nullopt;
    }
    const std::size_t end = std::min(datagram.size(), headerBytes + word(datagram, 2));
    std::optional<stream::UdpAddress> plain;
    for (std::size_t at = headerBytes; at + 4 <= end;) {
        const std::uint16_t type = word(datagram, at);
        const std::size_t length = word(datagram, at + 2);
        if (at + 4 + length > end) {
            break;
        }
        const std::string_view value = datagram.substr(at + 4, length);
        if (type == xorMappedAddress) {
            return addressIn(value, true);
        }
        if (type == mappedAddress) {
            plain = addressIn(value, false);
        }
        at += 4 + ((length + 3) & ~std::size_t{3});
    }
    return plain;
}

} // namespace opensu::online
