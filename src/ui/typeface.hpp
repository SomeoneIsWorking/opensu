// Text drawing, owned in one place.
//
// iiSU's default typeface is Cal Sans (pp4.a), drawn a glyph at a time through gpu2d's text,
// unhinted, each glyph at the fractional pen its TextGrid places it at. Sizes are Android's: the em
// in pixels, with letter spacing in ems, so sizes recovered from iiSU apply unchanged.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/text.hpp"

#include "face_metrics.hpp"
#include "paint.hpp"
#include "text_grid.hpp"

namespace opensu::ui {

/// Owns the shell's typeface.
class Typeface {
  public:
    /// Faces are read from `assetsDir`; nothing is loaded until `load`.
    explicit Typeface(std::filesystem::path assetsDir);

    /// Loads the first usable face onto `device`. Throws gpu2d::GpuError when none loads.
    void load(SDL_GPUDevice& device);

    /// The height of the face's line box (ascent to descent) at a style.
    [[nodiscard]] float lineBox(const TextStyle& style) const noexcept;
    /// The em whose line box is `height` pixels tall.
    [[nodiscard]] float emForLineBox(float height) const noexcept;

    /// The width a label advances.
    [[nodiscard]] float measure(std::string_view text, const TextStyle& style) const noexcept;

    /// `text` cut to fit `room` pixels, ending in dots when it is cut.
    [[nodiscard]] std::string fitted(std::string text, float room, const TextStyle& style);

    /// `text` broken into lines no wider than `room`, at spaces; a word wider than `room` keeps a
    /// line of its own.
    [[nodiscard]] std::vector<std::string> wrapped(const std::string& text, float room,
                                                   const TextStyle& style) const;

    /// Draws a label from `x`, its line box centred on `centreY`.
    void drawCentred(Canvas& canvas, std::string_view text, float x, float centreY,
                     const TextStyle& style, Colour colour);

    /// Draws the first character of `character` with the middle of its ink on `centre`.
    void drawInkCentred(Canvas& canvas, std::string_view character, Point centre,
                        const TextStyle& style, Colour colour);

    /// Cal Sans's capital height as a share of the em (navigation.md §5.3).
    static constexpr float capHeightPerEm = 0.70f;
    /// Draws a label from `origin.x` with the tops of its capitals at `origin.y`.
    void drawFromCapTop(Canvas& canvas, std::string_view text, Point origin, const TextStyle& style,
                        Colour colour);

    /// Releases the labels the last frame did not draw. Once per frame, after it was submitted.
    void retire();

    /// The same face hinted and kerned, as the interface toolkit's screens draw it.
    [[nodiscard]] gpu2d::Text& interfaceText();

  private:
    [[nodiscard]] gpu2d::Text& text();

    std::filesystem::path assetsDir_;
    std::unique_ptr<gpu2d::Text> text_;
    std::unique_ptr<gpu2d::Text> interface_;
    /// The loaded face's metrics; until then a line box of one em with the baseline at 0.8 of it.
    FaceMetrics face_{
        .unitsPerEm = 1.0f, .ascent = 0.8f, .descent = -0.2f, .advances = {}, .missingAdvance = 0};
};

} // namespace opensu::ui
