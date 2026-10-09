#include "face_metrics.hpp"

#include <cstdint>

namespace opensu::ui {
namespace {

class BigEndian {
  public:
    explicit BigEndian(std::span<const std::byte> data) noexcept : data_{data} {
    }

    [[nodiscard]] bool has(std::size_t offset, std::size_t size) const noexcept {
        return offset <= data_.size() && size <= data_.size() - offset;
    }
    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept {
        return static_cast<std::uint16_t>((byte(offset) << 8U) | byte(offset + 1));
    }
    [[nodiscard]] std::int16_t i16(std::size_t offset) const noexcept {
        return static_cast<std::int16_t>(u16(offset));
    }
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept {
        return (static_cast<std::uint32_t>(u16(offset)) << 16U) | u16(offset + 2);
    }

  private:
    [[nodiscard]] unsigned byte(std::size_t offset) const noexcept {
        return std::to_integer<unsigned>(data_[offset]);
    }
    std::span<const std::byte> data_;
};

/// The offset of a table in the sfnt directory, if present and wholly inside the file.
std::optional<std::size_t> findTable(const BigEndian& file, std::uint32_t tag,
                                     std::size_t minSize) noexcept {
    // Offset table: sfntVersion, numTables, then 16-byte records from offset 12.
    if (!file.has(0, 12)) {
        return std::nullopt;
    }
    const std::uint16_t count = file.u16(4);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t record = 12 + i * 16;
        if (!file.has(record, 16)) {
            return std::nullopt;
        }
        if (file.u32(record) != tag) {
            continue;
        }
        const std::size_t offset = file.u32(record + 8);
        if (!file.has(offset, minSize)) {
            return std::nullopt;
        }
        return offset;
    }
    return std::nullopt;
}

constexpr std::uint32_t tagHead = 0x68656164; // 'head'
constexpr std::uint32_t tagHhea = 0x68686561; // 'hhea'

} // namespace

std::optional<FaceMetrics> readFaceMetrics(std::span<const std::byte> data) noexcept {
    const BigEndian file{data};
    // head.unitsPerEm is at 18; hhea ascender and descender at 4 and 6.
    const auto head = findTable(file, tagHead, 20);
    const auto hhea = findTable(file, tagHhea, 8);
    if (!head || !hhea) {
        return std::nullopt;
    }
    const std::uint16_t unitsPerEm = file.u16(*head + 18);
    const std::int16_t ascent = file.i16(*hhea + 4);
    const std::int16_t descent = file.i16(*hhea + 6);
    if (unitsPerEm == 0 || ascent <= descent) {
        return std::nullopt;
    }
    return FaceMetrics{static_cast<float>(unitsPerEm), static_cast<float>(ascent),
                       static_cast<float>(descent)};
}

} // namespace opensu::ui
