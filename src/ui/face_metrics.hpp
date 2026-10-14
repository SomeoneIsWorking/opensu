// face_metrics — the metrics a TrueType face declares in its head, hhea, hmtx and cmap tables.
//
// Android sizes text by the em; a line box runs from the hhea ascent to its descent. These convert
// one to the other, and give each character's advance.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>

namespace opensu::ui {

struct FaceMetrics {
    float unitsPerEm{};
    /// hhea ascender, positive up.
    float ascent{};
    /// hhea descender, negative below the baseline.
    float descent{};
    /// Each mapped character's advance, in font units.
    std::unordered_map<char32_t, std::uint16_t> advances;
    /// The missing glyph's advance, for a character the face does not map.
    std::uint16_t missingAdvance{};

    /// The line box of one em: (ascent - descent) / unitsPerEm.
    [[nodiscard]] float lineBoxPerEm() const noexcept {
        return (ascent - descent) / unitsPerEm;
    }
    /// `codepoint`'s advance in font units.
    [[nodiscard]] float advance(char32_t codepoint) const noexcept;
};

/// Reads head.unitsPerEm, hhea ascender/descender and the Unicode characters' hmtx advances from a
/// TrueType or OpenType file, or nothing when a table is missing or truncated.
[[nodiscard]] std::optional<FaceMetrics> readFaceMetrics(std::span<const std::byte> file);

} // namespace opensu::ui
