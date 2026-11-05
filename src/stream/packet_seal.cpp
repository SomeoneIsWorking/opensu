#include "packet_seal.hpp"

#include <array>

#include "fileio/little_endian.hpp"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::stream {
namespace {

constexpr std::uint8_t version = 3;
constexpr std::size_t counterAt = 2;
constexpr std::size_t counterBytes = 8;

/// The AEAD nonce for `counter`: four zero bytes, then the counter.
std::array<std::uint8_t, secrets::crypto::sealNonceBytes> nonceOf(std::uint64_t counter) {
    std::array<std::uint8_t, secrets::crypto::sealNonceBytes> nonce{};
    for (std::size_t i = 0; i < counterBytes; ++i) {
        nonce[secrets::crypto::sealNonceBytes - counterBytes + i] =
            static_cast<std::uint8_t>((counter >> (8 * i)) & 0xFFU);
    }
    return nonce;
}

bool knownKind(std::uint8_t kind) {
    return kind >= static_cast<std::uint8_t>(PacketKind::Ping) &&
           kind <= static_cast<std::uint8_t>(PacketKind::Mapping);
}

} // namespace

PacketSeal::PacketSeal(std::span<const std::uint8_t> key) : key_{key.begin(), key.end()} {
}

PacketSeal::~PacketSeal() {
    secrets::wipeBytes(key_.data(), key_.size());
}

std::string PacketSeal::seal(PacketKind kind, std::string_view payload) {
    const std::uint64_t counter = counter_++;
    std::string header;
    header.reserve(headerBytes);
    header.push_back(static_cast<char>(version));
    header.push_back(static_cast<char>(kind));
    fileio::appendLittleEndian(header, counter, counterBytes);
    return header + secrets::crypto::seal(key_, nonceOf(counter), header, payload);
}

PacketOpener::PacketOpener(std::span<const std::uint8_t> key) : key_{key.begin(), key.end()} {
}

PacketOpener::~PacketOpener() {
    secrets::wipeBytes(key_.data(), key_.size());
}

std::optional<Opened> PacketOpener::open(std::string_view datagram) {
    if (datagram.size() < sealOverhead || static_cast<std::uint8_t>(datagram[0]) != version ||
        !knownKind(static_cast<std::uint8_t>(datagram[1]))) {
        return std::nullopt;
    }
    const std::uint64_t counter = fileio::littleEndian(datagram, counterAt, counterBytes);
    if (!fresh(counter)) {
        return std::nullopt;
    }
    const std::string_view header = datagram.substr(0, headerBytes);
    std::optional<std::string> payload =
        secrets::crypto::open(key_, nonceOf(counter), header, datagram.substr(headerBytes));
    if (!payload) {
        return std::nullopt;
    }
    take(counter);
    return Opened{static_cast<PacketKind>(datagram[1]), counter, std::move(*payload)};
}

bool PacketOpener::fresh(std::uint64_t counter) const {
    if (!newest_ || counter > *newest_) {
        return true;
    }
    return *newest_ - counter < window && !seen_[counter % window];
}

void PacketOpener::take(std::uint64_t counter) {
    if (!newest_ || counter > *newest_) {
        if (!newest_ || counter - *newest_ >= window) {
            seen_.reset();
        } else {
            for (std::uint64_t skipped = *newest_ + 1; skipped < counter; ++skipped) {
                seen_.reset(skipped % window);
            }
        }
        newest_ = counter;
    }
    seen_.set(counter % window);
}

} // namespace opensu::stream
