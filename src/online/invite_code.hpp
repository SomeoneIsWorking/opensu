// invite_code — what two people carry across a chat app in place of a server: the host's invite,
// with the secret and where the host may be reached, and when that is not enough the friend's
// reply, with where the friend may be. Crockford base32 with a prefix; whitespace, dashes and case
// are ignored on the way in, as a chat app may change them.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "secrets/crypto.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::online {

/// How many of each kind of address a code holds.
inline constexpr std::size_t mostAddresses = 3;
/// An invite's secret is this long.
inline constexpr std::size_t secretBytes = 16;

/// Where one machine may be reached.
struct Reach {
    /// The local UDP port.
    std::uint16_t port{0};
    /// As seen from the internet: STUN's and the router's mapping.
    std::vector<stream::UdpAddress> publics;
    /// This machine's interfaces, in network byte order: the LAN, and Tailscale's when it runs.
    std::vector<std::uint32_t> locals;
    /// The NAT gives each destination another port, which no other machine can guess.
    bool varying{false};

    /// Every address to try.
    [[nodiscard]] std::vector<stream::UdpAddress> addresses() const;

    bool operator==(const Reach&) const = default;
};

struct Invite {
    secrets::crypto::Bytes secret;
    Reach reach;
};

/// Why a pasted code was not taken.
enum class CodeProblem : std::uint8_t {
    Empty,
    /// It is no openSU code.
    Unknown,
    /// An invite where a reply belongs, or the other way round.
    WrongKind,
    /// A letter is wrong or missing.
    Damaged,
    /// From a newer openSU.
    Version,
    /// A reply, but to another invite.
    OtherInvite,
};

/// The problem in words, for the player.
[[nodiscard]] std::string_view textOf(CodeProblem problem) noexcept;

[[nodiscard]] std::string writeInvite(const Invite& invite);
/// Nothing, with `problem`, when `text` is not an invite.
[[nodiscard]] std::optional<Invite> readInvite(std::string_view text, CodeProblem& problem);

/// `reach` as a reply to the invite with `secret`.
[[nodiscard]] std::string writeReply(const Reach& reach, std::span<const std::uint8_t> secret);
/// Nothing, with `problem`, when `text` is not a reply to the invite with `secret`.
[[nodiscard]] std::optional<Reach>
readReply(std::string_view text, std::span<const std::uint8_t> secret, CodeProblem& problem);

/// `code` in groups of four, as the screen shows it; reading takes either form.
[[nodiscard]] std::string spaced(std::string_view code);

} // namespace opensu::online
