#include "face_metrics.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

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
constexpr std::uint32_t tagCmap = 0x636D6170; // 'cmap'

/// Format 4: segments of 16-bit ranges; a range maps when its delta or glyph array gives a glyph.
bool readFormat4(const BigEndian& file, std::size_t table, std::vector<int>& out) {
    if (!file.has(table, 14)) {
        return false;
    }
    const std::size_t segments = file.u16(table + 6) / 2U;
    const std::size_t ends = table + 14;
    const std::size_t starts = ends + segments * 2 + 2;
    const std::size_t deltas = starts + segments * 2;
    const std::size_t offsets = deltas + segments * 2;
    if (!file.has(ends, segments * 8 + 2)) {
        return false;
    }
    for (std::size_t i = 0; i < segments; ++i) {
        const unsigned end = file.u16(ends + i * 2);
        const unsigned start = file.u16(starts + i * 2);
        const unsigned delta = file.u16(deltas + i * 2);
        const unsigned rangeOffset = file.u16(offsets + i * 2);
        for (unsigned c = start; c <= end && c != 0xFFFFU; ++c) {
            unsigned glyph = 0;
            if (rangeOffset == 0) {
                glyph = (c + delta) & 0xFFFFU;
            } else {
                const std::size_t at =
                    offsets + i * 2 + rangeOffset + static_cast<std::size_t>(c - start) * 2;
                if (!file.has(at, 2)) {
                    return false;
                }
                glyph = file.u16(at);
                glyph = glyph == 0 ? 0 : ((glyph + delta) & 0xFFFFU);
            }
            if (glyph != 0) {
                out.push_back(static_cast<int>(c));
            }
        }
    }
    return true;
}

/// Format 12: groups of 32-bit ranges, each mapping to consecutive glyphs.
bool readFormat12(const BigEndian& file, std::size_t table, std::vector<int>& out) {
    if (!file.has(table, 16)) {
        return false;
    }
    const std::size_t groups = file.u32(table + 12);
    if (!file.has(table + 16, groups * 12)) {
        return false;
    }
    constexpr std::uint32_t lastUnicode = 0x10FFFF;
    for (std::size_t i = 0; i < groups; ++i) {
        const std::size_t group = table + 16 + i * 12;
        const std::uint32_t start = file.u32(group);
        const std::uint32_t end = std::min(file.u32(group + 4), lastUnicode);
        const std::uint32_t glyph = file.u32(group + 8);
        for (std::uint32_t c = start; c <= end; ++c) {
            if (glyph + (c - start) != 0) {
                out.push_back(static_cast<int>(c));
            }
        }
    }
    return true;
}

/// The Unicode subtable's characters: full-repertoire format 12 first, else BMP format 4.
std::optional<std::vector<int>> readCodepoints(const BigEndian& file) {
    const auto cmap = findTable(file, tagCmap, 4);
    if (!cmap) {
        return std::nullopt;
    }
    const std::size_t count = file.u16(*cmap + 2);
    if (!file.has(*cmap + 4, count * 8)) {
        return std::nullopt;
    }
    std::optional<std::size_t> bmp;
    std::optional<std::size_t> full;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t record = *cmap + 4 + i * 8;
        const std::uint16_t platform = file.u16(record);
        const std::uint16_t encoding = file.u16(record + 2);
        const std::size_t table = *cmap + file.u32(record + 4);
        if (!file.has(table, 2)) {
            continue;
        }
        const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
        const std::uint16_t format = file.u16(table);
        if (unicode && format == 12) {
            full = table;
        } else if (unicode && format == 4) {
            bmp = table;
        }
    }
    std::vector<int> codepoints;
    const bool read =
        full ? readFormat12(file, *full, codepoints) : bmp && readFormat4(file, *bmp, codepoints);
    if (!read) {
        return std::nullopt;
    }
    std::ranges::sort(codepoints);
    const auto [first, last] = std::ranges::unique(codepoints);
    codepoints.erase(first, last);
    return codepoints;
}

} // namespace

std::optional<FaceMetrics> readFaceMetrics(std::span<const std::byte> data) {
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
    std::optional<std::vector<int>> codepoints = readCodepoints(file);
    if (unitsPerEm == 0 || ascent <= descent || !codepoints || codepoints->empty()) {
        return std::nullopt;
    }
    return FaceMetrics{static_cast<float>(unitsPerEm), static_cast<float>(ascent),
                       static_cast<float>(descent), std::move(*codepoints)};
}

} // namespace opensu::ui
