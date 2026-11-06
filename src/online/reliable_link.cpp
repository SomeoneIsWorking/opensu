#include "reliable_link.hpp"

#include <algorithm>

#include "fileio/little_endian.hpp"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;

enum class Datagram : std::uint8_t {
    /// A sequence number and a piece of the stream.
    Data = 1,
    /// The first sequence number not yet received.
    Ack = 2,
    /// The sender's clock in microseconds, to send back.
    Ping = 3,
    Pong = 4,
};

constexpr std::size_t dataHeader = 1 + 4;
constexpr std::size_t ackBytes = 1 + 4;
constexpr std::size_t pingBytes = 1 + 8;
constexpr std::size_t messageHeader = 1 + 4;
constexpr auto resendAfter = 200ms;
constexpr auto pingEvery = 500ms;
constexpr auto silentAfter = 15s;
constexpr std::size_t askCap = std::size_t{64} * 1024;
constexpr std::size_t answerCap = 512;
constexpr std::size_t saveCap = std::size_t{4} * 1024 * 1024;
constexpr std::size_t goCap = 32;

bool knownKind(std::uint8_t kind) noexcept {
    return kind >= static_cast<std::uint8_t>(LinkKind::Ask) &&
           kind <= static_cast<std::uint8_t>(LinkKind::Go);
}

std::uint64_t microsecondsNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          ReliableLink::Clock::now().time_since_epoch())
                                          .count());
}

} // namespace

std::size_t linkCap(LinkKind kind) noexcept {
    switch (kind) {
    case LinkKind::Ask:
        return askCap;
    case LinkKind::Answer:
        return answerCap;
    case LinkKind::Save:
        return saveCap;
    case LinkKind::Go:
        return goCap;
    }
    return 0;
}

ReliableLink::ReliableLink(const stream::UdpSocket& socket, stream::UdpAddress peer,
                           std::span<const std::uint8_t> sendKey,
                           std::span<const std::uint8_t> receiveKey)
    : socket_{socket}, peer_{peer}, seal_{sendKey}, opener_{receiveKey},
      buffer_(stream::largestDatagram) {
}

void ReliableLink::send(LinkKind kind, std::string_view body) {
    outgoing_.push_back(static_cast<char>(kind));
    fileio::appendLittleEndian(outgoing_, body.size(), 4);
    outgoing_.append(body);
}

void ReliableLink::sendData(std::uint32_t sequence, std::string_view chunk) {
    std::string payload;
    payload.reserve(dataHeader + chunk.size());
    payload.push_back(static_cast<char>(Datagram::Data));
    fileio::appendLittleEndian(payload, sequence, 4);
    payload.append(chunk);
    socket_.send(peer_, seal_.seal(stream::PacketKind::Netplay, payload));
}

void ReliableLink::sendAck() {
    std::string payload(1, static_cast<char>(Datagram::Ack));
    fileio::appendLittleEndian(payload, expected_, 4);
    socket_.send(peer_, seal_.seal(stream::PacketKind::Netplay, payload));
}

void ReliableLink::sendPing(Clock::time_point now) {
    pinged_ = now;
    std::string payload(1, static_cast<char>(Datagram::Ping));
    fileio::appendLittleEndian(payload, microsecondsNow(), 8);
    socket_.send(peer_, seal_.seal(stream::PacketKind::Netplay, payload));
}

void ReliableLink::sendChunks() {
    const Clock::time_point now = Clock::now();
    for (Unacked& waiting : unacked_) {
        if (now - waiting.sent >= resendAfter) {
            sendData(waiting.sequence, waiting.chunk);
            waiting.sent = now;
        }
    }
    while (unacked_.size() < window && !outgoing_.empty()) {
        const std::size_t size = std::min(chunkBytes, outgoing_.size());
        Unacked chunk{.sequence = nextSequence_++, .chunk = outgoing_.substr(0, size), .sent = now};
        outgoing_.erase(0, size);
        sendData(chunk.sequence, chunk.chunk);
        unacked_.push_back(std::move(chunk));
    }
    if (now - pinged_ >= pingEvery) {
        sendPing(now);
    }
}

void ReliableLink::pump(std::chrono::milliseconds wait) {
    if (broken_) {
        return;
    }
    sendChunks();
    stream::UdpAddress from;
    std::optional<std::size_t> length = socket_.receive(buffer_, from, wait);
    while (length) {
        if (std::optional<stream::Opened> opened = opener_.open({buffer_.data(), *length});
            opened && opened->kind == stream::PacketKind::Netplay) {
            peer_ = from;
            heard_ = Clock::now();
            takeDatagram(opened->payload);
        }
        length = socket_.receive(buffer_, from, 0ms);
    }
}

void ReliableLink::takeDatagram(std::string_view payload) {
    if (payload.empty()) {
        return;
    }
    switch (static_cast<Datagram>(payload.front())) {
    case Datagram::Data:
        takeData(payload);
        break;
    case Datagram::Ack:
        takeAck(payload);
        break;
    case Datagram::Ping:
        if (payload.size() == pingBytes) {
            std::string pong{payload};
            pong.front() = static_cast<char>(Datagram::Pong);
            socket_.send(peer_, seal_.seal(stream::PacketKind::Netplay, pong));
        }
        break;
    case Datagram::Pong:
        takePong(payload);
        break;
    }
}

void ReliableLink::takeData(std::string_view payload) {
    if (payload.size() < dataHeader || payload.size() > dataHeader + chunkBytes) {
        broken_ = true;
        return;
    }
    const auto sequence = static_cast<std::uint32_t>(fileio::littleEndian(payload, 1, 4));
    if (sequence >= expected_ && sequence - expected_ < window) {
        early_.try_emplace(sequence, payload.substr(dataHeader));
    }
    for (auto next = early_.find(expected_); next != early_.end(); next = early_.find(expected_)) {
        incoming_.append(next->second);
        early_.erase(next);
        ++expected_;
    }
    parse();
    sendAck();
}

void ReliableLink::takeAck(std::string_view payload) {
    if (payload.size() != ackBytes) {
        return;
    }
    const auto acked = static_cast<std::uint32_t>(fileio::littleEndian(payload, 1, 4));
    while (!unacked_.empty() && unacked_.front().sequence < acked) {
        unacked_.pop_front();
    }
}

void ReliableLink::takePong(std::string_view payload) {
    if (payload.size() != pingBytes) {
        return;
    }
    const std::uint64_t sent = fileio::littleEndian(payload, 1, 8);
    const std::uint64_t now = microsecondsNow();
    if (sent <= now) {
        roundTrip_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds{now - sent});
    }
}

void ReliableLink::parse() {
    while (incoming_.size() >= 1 && !broken_) {
        const auto kind = static_cast<std::uint8_t>(incoming_.front());
        if (!knownKind(kind)) {
            broken_ = true;
            return;
        }
        if (incoming_.size() < messageHeader) {
            return;
        }
        const std::uint64_t size = fileio::littleEndian(incoming_, 1, 4);
        if (size > linkCap(static_cast<LinkKind>(kind))) {
            broken_ = true;
            return;
        }
        if (incoming_.size() < messageHeader + size) {
            return;
        }
        arrived_.push_back({static_cast<LinkKind>(kind), incoming_.substr(messageHeader, size)});
        incoming_.erase(0, messageHeader + size);
    }
}

std::optional<LinkMessage> ReliableLink::take() {
    if (arrived_.empty()) {
        return std::nullopt;
    }
    LinkMessage message = std::move(arrived_.front());
    arrived_.pop_front();
    return message;
}

bool ReliableLink::silent() const noexcept {
    return Clock::now() - heard_ > silentAfter;
}

} // namespace opensu::online
