// face_metrics — the vertical metrics a TrueType face declares in its head and hhea tables, and
// the characters its cmap maps.
//
// Android sizes text by the em; stb_truetype, under raylib, sizes it by ascent minus descent.
// These are what convert one to the other.
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace opensu::ui {

struct FaceMetrics {
    float unitsPerEm{};
    /// hhea ascender, positive up.
    float ascent{};
    /// hhea descender, negative below the baseline.
    float descent{};
    /// Every character the cmap maps to a glyph, ascending; the atlas loads exactly these.
    std::vector<int> codepoints;

    /// The line box stb_truetype fills for one em: (ascent - descent) / unitsPerEm.
    [[nodiscard]] float lineBoxPerEm() const noexcept {
        return (ascent - descent) / unitsPerEm;
    }
};

/// Reads head.unitsPerEm, hhea ascender/descender and the Unicode cmap (format 4 or 12) from a
/// TrueType or OpenType file, or nothing when a table is missing or truncated.
[[nodiscard]] std::optional<FaceMetrics> readFaceMetrics(std::span<const std::byte> file);

} // namespace opensu::ui
