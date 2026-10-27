#include "text_grid.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::ui {
namespace {

constexpr char32_t replacement = 0xFFFD;

} // namespace

std::pair<char32_t, std::size_t> firstCodepoint(std::string_view text) noexcept {
    const auto lead = static_cast<unsigned char>(text[0]);
    std::size_t length = 1;
    char32_t codepoint = lead;
    if (lead >= 0xF0U) {
        length = 4;
        codepoint = lead & 0x07U;
    } else if (lead >= 0xE0U) {
        length = 3;
        codepoint = lead & 0x0FU;
    } else if (lead >= 0xC0U) {
        length = 2;
        codepoint = lead & 0x1FU;
    } else if (lead >= 0x80U) {
        return {replacement, 1};
    }
    if (length > text.size()) {
        return {replacement, 1};
    }
    for (std::size_t i = 1; i < length; ++i) {
        const auto next = static_cast<unsigned char>(text[i]);
        if ((next & 0xC0U) != 0x80U) {
            return {replacement, 1};
        }
        codepoint = (codepoint << 6U) | (next & 0x3FU);
    }
    return {codepoint, length};
}

TextGrid::TextGrid(const FaceMetrics& face, const TextStyle& style) noexcept
    : face_{face}, lineBox_{style.size * face.lineBoxPerEm()},
      tracking_{style.tracking * style.size} {
    const float wholeBox = std::max(std::round(lineBox_), 1.0f);
    em_ = wholeBox / face.lineBoxPerEm();
    pixelsPerUnit_ = wholeBox / (face.ascent - face.descent);
    stretch_ = lineBox_ / wholeBox;
}

float TextGrid::ascent() const noexcept {
    return std::trunc(face_.ascent * pixelsPerUnit_) * stretch_;
}

float TextGrid::advance(char32_t codepoint) const noexcept {
    return face_.advance(codepoint) * pixelsPerUnit_ * stretch_ + tracking_;
}

float TextGrid::measure(std::string_view text) const noexcept {
    float width = 0.0f;
    forEachCodepoint(text, [&](char32_t codepoint, std::string_view) {
        width += advance(codepoint);
    });
    return width;
}

} // namespace opensu::ui
