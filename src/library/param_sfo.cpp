#include "param_sfo.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace opensu::library::roms {
namespace {

constexpr std::string_view magic{"\0PSF", 4};
constexpr std::size_t headerSize = 20;
constexpr std::size_t entrySize = 16;
constexpr std::uint16_t utf8Special = 0x0004;
constexpr std::uint16_t utf8 = 0x0204;

std::optional<std::uint32_t> littleEndian(std::string_view bytes, std::size_t at,
                                          std::size_t width) {
    if (at + width > bytes.size()) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
    }
    return value;
}

} // namespace

std::map<std::string, std::string> sfoStrings(std::string_view bytes) {
    std::map<std::string, std::string> out;
    if (!bytes.starts_with(magic)) {
        return out;
    }
    const auto keyTable = littleEndian(bytes, 8, 4);
    const auto dataTable = littleEndian(bytes, 12, 4);
    const auto entries = littleEndian(bytes, 16, 4);
    if (!keyTable || !dataTable || !entries) {
        return out;
    }
    for (std::size_t i = 0; i < *entries; ++i) {
        const std::size_t entry = headerSize + i * entrySize;
        const auto keyOffset = littleEndian(bytes, entry, 2);
        const auto format = littleEndian(bytes, entry + 2, 2);
        const auto length = littleEndian(bytes, entry + 4, 4);
        const auto dataOffset = littleEndian(bytes, entry + 12, 4);
        if (!keyOffset || !format || !length || !dataOffset) {
            return {};
        }
        if (*format != utf8 && *format != utf8Special) {
            continue;
        }
        const std::size_t keyAt = std::size_t{*keyTable} + *keyOffset;
        const std::size_t keyEnd = bytes.find('\0', keyAt);
        const std::size_t valueAt = std::size_t{*dataTable} + *dataOffset;
        if (keyAt >= bytes.size() || keyEnd == std::string_view::npos ||
            valueAt + *length > bytes.size()) {
            return {};
        }
        std::string_view value = bytes.substr(valueAt, *length);
        value = value.substr(0, value.find('\0'));
        out.emplace(std::string{bytes.substr(keyAt, keyEnd - keyAt)}, std::string{value});
    }
    return out;
}

} // namespace opensu::library::roms
