#include "secret.hpp"

#include <algorithm>
#include <cctype>

#include <openssl/crypto.h>

namespace opensu::secrets {
namespace {

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const int lowered = std::tolower(static_cast<unsigned char>(c));
    if (lowered >= 'a' && lowered <= 'f') {
        return lowered - 'a' + 10;
    }
    return -1;
}

} // namespace

void wipeBytes(void* data, std::size_t size) noexcept {
    OPENSSL_cleanse(data, size);
}

std::optional<std::vector<std::uint8_t>> hexBytes(std::string_view hex) {
    if (hex.empty() || hex.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int high = hexDigit(hex[i]);
        const int low = hexDigit(hex[i + 1]);
        if (high < 0 || low < 0) {
            wipeBytes(bytes.data(), bytes.size());
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

bool isLowerHex(std::string_view text) noexcept {
    return !text.empty() && std::ranges::all_of(text, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string hexText(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0FU]);
    }
    return out;
}

} // namespace opensu::secrets
