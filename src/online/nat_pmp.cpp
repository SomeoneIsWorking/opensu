#include "nat_pmp.hpp"

#include <array>
#include <cstring>

#include "host_network.hpp"

namespace opensu::online {
namespace {

using namespace std::chrono_literals;

constexpr std::uint8_t opExternalAddress = 0;
constexpr std::uint8_t opMapUdp = 1;
constexpr std::uint8_t answerBit = 128;
constexpr std::size_t addressAnswerBytes = 12;
constexpr std::size_t mapAnswerBytes = 16;
constexpr std::array waits{250ms, 500ms, 1000ms};

std::uint16_t word(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(bytes[at]) << 8U) |
                                      static_cast<std::uint8_t>(bytes[at + 1]));
}

std::uint32_t longWord(std::string_view bytes, std::size_t at) {
    return (std::uint32_t{word(bytes, at)} << 16U) | word(bytes, at + 2);
}

void appendWord(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value >> 8U));
    bytes.push_back(static_cast<char>(value & 0xFFU));
}

std::string mapRequest(std::uint16_t port, std::uint16_t external, std::chrono::seconds lease) {
    std::string request;
    request.push_back(0);
    request.push_back(static_cast<char>(opMapUdp));
    appendWord(request, 0);
    appendWord(request, port);
    appendWord(request, external);
    const auto seconds = static_cast<std::uint32_t>(lease.count());
    appendWord(request, static_cast<std::uint16_t>(seconds >> 16U));
    appendWord(request, static_cast<std::uint16_t>(seconds & 0xFFFFU));
    return request;
}

} // namespace

NatPmp::NatPmp(stream::UdpAddress gateway) : gateway_{gateway}, socket_{0} {
}

std::optional<std::string> NatPmp::ask(std::string_view request, std::size_t answerBytes) {
    std::array<char, 64> buffer{};
    const auto op = static_cast<std::uint8_t>(request[1]);
    for (const std::chrono::milliseconds wait : waits) {
        socket_.send(gateway_, request);
        const auto until = std::chrono::steady_clock::now() + wait;
        while (std::chrono::steady_clock::now() < until) {
            stream::UdpAddress from;
            const std::optional<std::size_t> length = socket_.receive(buffer, from, wait);
            if (!length || !(from == gateway_) || *length < answerBytes) {
                continue;
            }
            const std::string_view answer{buffer.data(), *length};
            if (static_cast<std::uint8_t>(answer[1]) != (op | answerBit)) {
                continue;
            }
            if (word(answer, 2) != 0) {
                return std::nullopt;
            }
            return std::string{answer};
        }
    }
    return std::nullopt;
}

std::optional<Mapping> NatPmp::open(std::uint16_t port, std::chrono::seconds lease) {
    const std::string addressRequest{"\0\0", 2};
    static_assert(opExternalAddress == 0);
    const std::optional<std::string> address = ask(addressRequest, addressAnswerBytes);
    if (!address) {
        return std::nullopt;
    }
    Mapping mapping;
    std::memcpy(&mapping.external.ip, address->data() + 8, sizeof mapping.external.ip);
    if (privateAddress(mapping.external.ip)) {
        return std::nullopt;
    }
    const std::uint16_t wanted = external_ != 0 ? external_ : port;
    const std::optional<std::string> mapped = ask(mapRequest(port, wanted, lease), mapAnswerBytes);
    if (!mapped || word(*mapped, 8) != port) {
        return std::nullopt;
    }
    external_ = word(*mapped, 10);
    mapping.external.port = external_;
    mapping.lease = std::chrono::seconds{longWord(*mapped, 12)};
    if (mapping.lease == 0s || mapping.lease > longestLease) {
        close(port);
        return std::nullopt;
    }
    return mapping;
}

void NatPmp::close(std::uint16_t port) {
    static_cast<void>(ask(mapRequest(port, 0, 0s), mapAnswerBytes));
    external_ = 0;
}

} // namespace opensu::online
