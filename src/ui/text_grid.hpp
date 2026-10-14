// text_grid — where a label's glyphs go, on the pixel grid the shell's stb_truetype atlas laid
// them on: the face sized so its line box is whole pixels, each advance cut to whole pixels at
// that size and stretched back to the style's line box, the baseline likewise.
#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

#include "face_metrics.hpp"

namespace opensu::ui {

/// A text size as Android takes it.
struct TextStyle {
    /// The em, in pixels.
    float size{};
    /// Letter spacing, in ems, added half before and half after each glyph.
    float tracking{};
};

/// The codepoint at the start of UTF-8 `text` (not empty) and its byte count; a malformed byte
/// reads as U+FFFD.
[[nodiscard]] std::pair<char32_t, std::size_t> firstCodepoint(std::string_view text) noexcept;

/// Calls `visit(codepoint, bytes)` for each character of UTF-8 `text`.
template <typename Visit> void forEachCodepoint(std::string_view text, Visit visit) {
    while (!text.empty()) {
        const auto [codepoint, length] = firstCodepoint(text);
        visit(codepoint, text.substr(0, length));
        text.remove_prefix(length);
    }
}

/// One style of one face on the pixel grid.
class TextGrid {
  public:
    TextGrid(const FaceMetrics& face, const TextStyle& style) noexcept;

    /// The em the glyphs are rasterised at: the one whose line box is whole pixels.
    [[nodiscard]] float em() const noexcept {
        return em_;
    }
    /// The style's line box, ascent to descent.
    [[nodiscard]] float lineBox() const noexcept {
        return lineBox_;
    }
    /// The baseline's depth below the line box's top.
    [[nodiscard]] float ascent() const noexcept;
    /// How far `codepoint` moves the pen, letter spacing included.
    [[nodiscard]] float advance(char32_t codepoint) const noexcept;
    /// How far the pen moves before the first glyph.
    [[nodiscard]] float lead() const noexcept {
        return tracking_ * 0.5f;
    }
    /// The width `text` advances.
    [[nodiscard]] float measure(std::string_view text) const noexcept;

  private:
    const FaceMetrics& face_;
    float lineBox_{};
    float em_{};
    /// Pixels per font unit at the whole-pixel line box.
    float pixelsPerUnit_{};
    /// The style's line box over the whole-pixel one.
    float stretch_{};
    float tracking_{};
};

} // namespace opensu::ui
