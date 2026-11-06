#include "state_sync.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include "fileio/little_endian.hpp"
#include "lucent/log.h"
#include "secrets/crypto.hpp"

namespace opensu::netplay {
namespace {

using namespace std::chrono_literals;

// Above `Session`'s kinds, which it ignores.
enum class Message : std::uint8_t {
    /// The state's size and SHA-256.
    Offer = 16,
    /// The first chunk the sender lacks.
    Need = 17,
    /// A chunk's index and bytes.
    Chunk = 18,
    /// The sender has what it needs.
    Done = 19,
};

constexpr std::size_t hashBytes = 32;
constexpr std::size_t offerBytes = 1 + 8 + hashBytes;
constexpr std::size_t needBytes = 1 + 4;
constexpr std::size_t chunkHeader = 1 + 4;
constexpr std::uint32_t window = 32;
constexpr auto resend = 50ms;
constexpr int doneRepeats = 3;

std::array<std::uint8_t, hashBytes> hashOf(std::span<const std::byte> state) {
    const secrets::crypto::Bytes digest = secrets::crypto::sha256(
        std::string_view{reinterpret_cast<const char*>(state.data()), state.size()});
    std::array<std::uint8_t, hashBytes> hash{};
    std::ranges::copy(digest, hash.begin());
    return hash;
}

std::uint32_t chunksOf(std::size_t size) {
    return static_cast<std::uint32_t>((size + StateSync::chunkBytes - 1) / StateSync::chunkBytes);
}

} // namespace

StateSync::StateSync(Transport& transport, Role role, std::span<const std::byte> own)
    : transport_{transport}, role_{role}, own_{own}, hash_{hashOf(own)} {
}

void StateSync::finish(Outcome outcome) {
    outcome_ = outcome;
    if (role_ == Role::Takes) {
        const std::string done(1, static_cast<char>(Message::Done));
        for (int times = 0; times < doneRepeats; ++times) {
            transport_.send(done);
        }
    }
}

void StateSync::takeOffer(std::string_view message) {
    if (role_ != Role::Takes || offered_ || message.size() != offerBytes) {
        return;
    }
    const std::uint64_t size = fileio::littleEndian(message, 1, 8);
    if (size != own_.size()) {
        lucent::error("netplay", "the other machine's state is {} bytes, this core's {}", size,
                      own_.size());
        finish(Outcome::Refused);
        return;
    }
    offered_ = true;
    std::array<std::uint8_t, hashBytes> theirs{};
    std::memcpy(theirs.data(), message.data() + 9, hashBytes);
    if (theirs == hash_) {
        finish(Outcome::Same);
        return;
    }
    hash_ = theirs;
    received_.resize(own_.size());
    ask();
}

void StateSync::takeChunk(std::string_view message) {
    if (role_ != Role::Takes || !offered_ || message.size() <= chunkHeader) {
        return;
    }
    const auto index = static_cast<std::uint32_t>(fileio::littleEndian(message, 1, 4));
    const std::size_t at = std::size_t{index} * chunkBytes;
    const std::size_t bytes = message.size() - chunkHeader;
    if (index != next_ || at >= received_.size() ||
        bytes != std::min(chunkBytes, received_.size() - at)) {
        return;
    }
    std::memcpy(received_.data() + at, message.data() + chunkHeader, bytes);
    ++next_;
    if (next_ < chunksOf(received_.size())) {
        if (next_ % window == 0) {
            ask();
        }
        return;
    }
    if (hashOf(received_) != hash_) {
        lucent::error("netplay", "the other machine's state is not the one it offered");
        received_.clear();
        finish(Outcome::Refused);
        return;
    }
    finish(Outcome::Received);
}

void StateSync::take(std::string_view message) {
    if (message.empty()) {
        return;
    }
    heard_ = Clock::now();
    const auto kind = static_cast<Message>(message.front());
    if (kind == Message::Offer) {
        takeOffer(message);
    } else if (kind == Message::Chunk) {
        takeChunk(message);
    } else if (role_ == Role::Offers && kind == Message::Need && message.size() == needBytes) {
        offered_ = true;
        next_ = std::max(next_, static_cast<std::uint32_t>(fileio::littleEndian(message, 1, 4)));
        sendWindow();
    } else if (role_ == Role::Offers && (kind == Message::Done || message.front() < 16)) {
        // A session's own message says the other machine has gone on.
        outcome_ = Outcome::Same;
    }
}

void StateSync::ask() {
    std::string message(1, static_cast<char>(Message::Need));
    fileio::appendLittleEndian(message, next_, 4);
    transport_.send(message);
    sent_ = Clock::now();
}

void StateSync::sendWindow() {
    const std::uint32_t chunks = chunksOf(own_.size());
    const std::uint32_t last = std::min(next_ + window, chunks);
    for (std::uint32_t index = next_; index < last; ++index) {
        const std::size_t at = std::size_t{index} * chunkBytes;
        const std::size_t bytes = std::min(chunkBytes, own_.size() - at);
        std::string message(1, static_cast<char>(Message::Chunk));
        fileio::appendLittleEndian(message, index, 4);
        message.append(reinterpret_cast<const char*>(own_.data() + at), bytes);
        transport_.send(message);
    }
    sent_ = Clock::now();
}

void StateSync::sendDue() {
    if (Clock::now() - sent_ < resend) {
        return;
    }
    if (role_ == Role::Takes) {
        if (offered_) {
            ask();
        }
        return;
    }
    std::string offer(1, static_cast<char>(Message::Offer));
    fileio::appendLittleEndian(offer, own_.size(), 8);
    offer.append(reinterpret_cast<const char*>(hash_.data()), hash_.size());
    transport_.send(offer);
    sent_ = Clock::now();
}

StateSync::Outcome StateSync::pump(std::chrono::milliseconds wait) {
    if (outcome_ != Outcome::Running) {
        return outcome_;
    }
    sendDue();
    std::optional<std::string> message = transport_.receive(wait);
    while (message && outcome_ == Outcome::Running) {
        take(*message);
        message = transport_.receive(0ms);
    }
    if (outcome_ == Outcome::Running && Clock::now() - heard_ > patience) {
        outcome_ = Outcome::Lapsed;
    }
    return outcome_;
}

} // namespace opensu::netplay
