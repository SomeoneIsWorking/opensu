#include "invite_code.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace opensu::online {
namespace {

constexpr std::string_view invitePrefix = "OSU";
constexpr std::string_view replyPrefix = "OSR";
constexpr std::string_view alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::uint8_t version = 1;
constexpr std::size_t checkBytes = 2;
constexpr std::size_t tagBytes = 8;
constexpr std::uint8_t flagVarying = 1;
constexpr std::size_t publicBytes = 6;
constexpr std::size_t localBytes = 4;
constexpr std::size_t groupChars = 4;
constexpr std::size_t prefixChars = 3;
constexpr std::string_view replyLabel = "opensu-online\nreply";

using Bytes = secrets::crypto::Bytes;

std::string base32(std::span<const std::uint8_t> bytes) {
    std::string text;
    text.reserve((bytes.size() * 8 + 4) / 5);
    std::uint32_t held = 0;
    unsigned bits = 0;
    for (const std::uint8_t byte : bytes) {
        held = (held << 8U) | byte;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            text.push_back(alphabet[(held >> bits) & 31U]);
        }
    }
    if (bits > 0) {
        text.push_back(alphabet[(held << (5 - bits)) & 31U]);
    }
    return text;
}

/// The value of `letter`, the letters a person may mistake for digits read as those digits.
std::optional<std::uint8_t> valueOf(char letter) {
    if (letter == 'O') {
        return std::uint8_t{0};
    }
    if (letter == 'I' || letter == 'L') {
        return std::uint8_t{1};
    }
    const std::size_t at = alphabet.find(letter);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(at);
}

std::optional<Bytes> bytesOf(std::string_view text) {
    Bytes bytes;
    bytes.reserve(text.size() * 5 / 8);
    std::uint32_t held = 0;
    unsigned bits = 0;
    for (const char letter : text) {
        const std::optional<std::uint8_t> value = valueOf(letter);
        if (!value) {
            return std::nullopt;
        }
        held = (held << 5U) | *value;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((held >> bits) & 0xFFU));
        }
    }
    return bytes;
}

/// `text` as it is written: upper case, without whatever a chat app put between its letters.
std::string cleaned(std::string_view text) {
    std::string clean;
    clean.reserve(text.size());
    for (const char letter : text) {
        const auto byte = static_cast<unsigned char>(letter);
        if (std::isalnum(byte) != 0) {
            clean.push_back(static_cast<char>(std::toupper(byte)));
        }
    }
    return clean;
}

void appendPort(Bytes& bytes, std::uint16_t port) {
    bytes.push_back(static_cast<std::uint8_t>(port >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(port & 0xFFU));
}

void appendIp(Bytes& bytes, std::uint32_t ip) {
    std::array<std::uint8_t, localBytes> raw{};
    std::memcpy(raw.data(), &ip, raw.size());
    bytes.insert(bytes.end(), raw.begin(), raw.end());
}

void appendReach(Bytes& bytes, const Reach& reach) {
    appendPort(bytes, reach.port);
    const std::size_t publics = std::min(reach.publics.size(), mostAddresses);
    const std::size_t locals = std::min(reach.locals.size(), mostAddresses);
    bytes.push_back(static_cast<std::uint8_t>((publics << 4U) | locals));
    for (std::size_t at = 0; at < publics; ++at) {
        appendIp(bytes, reach.publics[at].ip);
        appendPort(bytes, reach.publics[at].port);
    }
    for (std::size_t at = 0; at < locals; ++at) {
        appendIp(bytes, reach.locals[at]);
    }
}

std::uint16_t portAt(std::span<const std::uint8_t> bytes, std::size_t at) {
    return static_cast<std::uint16_t>((bytes[at] << 8U) | bytes[at + 1]);
}

std::uint32_t ipAt(std::span<const std::uint8_t> bytes, std::size_t at) {
    std::uint32_t ip = 0;
    std::memcpy(&ip, bytes.data() + at, sizeof ip);
    return ip;
}

/// The reach `bytes` holds entire, flagged by `flags`.
std::optional<Reach> reachIn(std::span<const std::uint8_t> bytes, std::uint8_t flags) {
    if (bytes.size() < 3) {
        return std::nullopt;
    }
    Reach reach;
    reach.port = portAt(bytes, 0);
    reach.varying = (flags & flagVarying) != 0;
    const std::size_t publics = bytes[2] >> 4U;
    const std::size_t locals = bytes[2] & 15U;
    if (publics > mostAddresses || locals > mostAddresses ||
        bytes.size() != 3 + publics * publicBytes + locals * localBytes) {
        return std::nullopt;
    }
    std::size_t at = 3;
    for (std::size_t index = 0; index < publics; ++index, at += publicBytes) {
        reach.publics.push_back({.ip = ipAt(bytes, at), .port = portAt(bytes, at + localBytes)});
    }
    for (std::size_t index = 0; index < locals; ++index, at += localBytes) {
        reach.locals.push_back(ipAt(bytes, at));
    }
    return reach;
}

Bytes checkOf(std::span<const std::uint8_t> bytes) {
    Bytes check = secrets::crypto::sha256(bytes);
    check.resize(checkBytes);
    return check;
}

Bytes tagOf(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> bytes) {
    std::string message{replyLabel};
    message.append(secrets::crypto::asText(bytes));
    Bytes tag = secrets::crypto::hmacSha256(secret, message);
    tag.resize(tagBytes);
    return tag;
}

/// A kind of code's prefix, and the other kind's.
struct Prefixes {
    std::string_view own;
    std::string_view other;
};

/// What `text` holds after its prefix; nothing, with `problem`, when it starts wrong.
std::optional<Bytes> body(std::string_view text, const Prefixes& prefixes, CodeProblem& problem) {
    const std::string clean = cleaned(text);
    if (clean.empty()) {
        problem = CodeProblem::Empty;
        return std::nullopt;
    }
    if (clean.starts_with(prefixes.other)) {
        problem = CodeProblem::WrongKind;
        return std::nullopt;
    }
    if (!clean.starts_with(prefixes.own)) {
        problem = CodeProblem::Unknown;
        return std::nullopt;
    }
    std::optional<Bytes> bytes = bytesOf(std::string_view{clean}.substr(prefixChars));
    if (!bytes || bytes->size() < 2) {
        problem = CodeProblem::Damaged;
        return std::nullopt;
    }
    return bytes;
}

} // namespace

std::vector<stream::UdpAddress> Reach::addresses() const {
    std::vector<stream::UdpAddress> all = publics;
    for (const std::uint32_t ip : locals) {
        all.push_back({.ip = ip, .port = port});
    }
    return all;
}

std::string_view textOf(CodeProblem problem) noexcept {
    switch (problem) {
    case CodeProblem::Empty:
        return "Paste the code first";
    case CodeProblem::Unknown:
        return "That is not an openSU code";
    case CodeProblem::WrongKind:
        return "That is the other kind of code: an invite goes in Join, a reply on the host's page";
    case CodeProblem::Damaged:
        return "That code is damaged: copy it again, whole";
    case CodeProblem::Version:
        return "That code is from a newer openSU: update this one";
    case CodeProblem::OtherInvite:
        return "That reply is to another invite";
    }
    return {};
}

std::string writeInvite(const Invite& invite) {
    Bytes bytes;
    bytes.push_back(version);
    bytes.push_back(invite.reach.varying ? flagVarying : std::uint8_t{0});
    bytes.insert(bytes.end(), invite.secret.begin(), invite.secret.end());
    appendReach(bytes, invite.reach);
    const Bytes check = checkOf(bytes);
    bytes.insert(bytes.end(), check.begin(), check.end());
    return std::string{invitePrefix} + base32(bytes);
}

std::optional<Invite> readInvite(std::string_view text, CodeProblem& problem) {
    const std::optional<Bytes> bytes =
        body(text, {.own = invitePrefix, .other = replyPrefix}, problem);
    if (!bytes) {
        return std::nullopt;
    }
    problem = CodeProblem::Damaged;
    if (bytes->size() < 2 + secretBytes + checkBytes) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> all{*bytes};
    const std::span<const std::uint8_t> kept = all.first(all.size() - checkBytes);
    if (!secrets::crypto::sameBytes(checkOf(kept), all.last(checkBytes))) {
        return std::nullopt;
    }
    if (all[0] != version) {
        problem = CodeProblem::Version;
        return std::nullopt;
    }
    std::optional<Reach> reach = reachIn(kept.subspan(2 + secretBytes), all[1]);
    if (!reach) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> secret = all.subspan(2, secretBytes);
    return Invite{.secret = Bytes{secret.begin(), secret.end()}, .reach = std::move(*reach)};
}

std::string writeReply(const Reach& reach, std::span<const std::uint8_t> secret) {
    Bytes bytes;
    bytes.push_back(version);
    bytes.push_back(reach.varying ? flagVarying : std::uint8_t{0});
    appendReach(bytes, reach);
    const Bytes tag = tagOf(secret, bytes);
    bytes.insert(bytes.end(), tag.begin(), tag.end());
    return std::string{replyPrefix} + base32(bytes);
}

std::optional<Reach> readReply(std::string_view text, std::span<const std::uint8_t> secret,
                               CodeProblem& problem) {
    const std::optional<Bytes> bytes =
        body(text, {.own = replyPrefix, .other = invitePrefix}, problem);
    if (!bytes) {
        return std::nullopt;
    }
    problem = CodeProblem::Damaged;
    if (bytes->size() < 2 + tagBytes) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> all{*bytes};
    const std::span<const std::uint8_t> kept = all.first(all.size() - tagBytes);
    if (!secrets::crypto::sameBytes(tagOf(secret, kept), all.last(tagBytes))) {
        problem = CodeProblem::OtherInvite;
        return std::nullopt;
    }
    if (all[0] != version) {
        problem = CodeProblem::Version;
        return std::nullopt;
    }
    return reachIn(kept.subspan(2), all[1]);
}

std::string spaced(std::string_view code) {
    std::string text;
    text.reserve(code.size() + code.size() / groupChars);
    for (std::size_t at = 0; at < code.size(); ++at) {
        if (at != 0 && at % groupChars == 0) {
            text.push_back(' ');
        }
        text.push_back(code[at]);
    }
    return text;
}

} // namespace opensu::online
