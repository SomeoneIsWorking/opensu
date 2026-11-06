// stun — the one STUN exchange openSU needs (RFC 8489): a Binding request, and the address a
// server saw it come from in the answer. Nothing else in a STUN message is read.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "stream/udp_socket.hpp"

namespace opensu::online {

inline constexpr std::size_t stunIdBytes = 12;
using StunId = std::array<std::uint8_t, stunIdBytes>;

/// A fresh transaction id.
[[nodiscard]] StunId newStunId();

/// A Binding request with `id`.
[[nodiscard]] std::string stunRequest(const StunId& id);

/// Whether `datagram` is STUN's, not a sealed one.
[[nodiscard]] bool isStun(std::string_view datagram) noexcept;

/// Where the answer to request `id` says the request came from; nothing for anything else.
[[nodiscard]] std::optional<stream::UdpAddress> stunMapped(std::string_view datagram,
                                                           const StunId& id);

} // namespace opensu::online
