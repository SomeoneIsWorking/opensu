// face_metrics — the vertical metrics a TrueType face declares in its head and hhea tables.
//
// Android sizes text by the em; stb_truetype, under raylib, sizes it by ascent minus descent.
// These are what convert one to the other.
#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace opensu::ui {

struct FaceMetrics {
    float unitsPerEm{};
    /// hhea ascender, positive up.
    float ascent{};
    /// hhea descender, negative below the baseline.
    float descent{};

    /// The line box stb_truetype fills for one em: (ascent - descent) / unitsPerEm.
    [[nodiscard]] float lineBoxPerEm() const noexcept {
        return (ascent - descent) / unitsPerEm;
    }
};

/// Reads head.unitsPerEm and hhea ascender/descender from a TrueType or OpenType file, or
/// nothing when the tables are missing or truncated.
[[nodiscard]] std::optional<FaceMetrics> readFaceMetrics(std::span<const std::byte> file) noexcept;

} // namespace opensu::ui
