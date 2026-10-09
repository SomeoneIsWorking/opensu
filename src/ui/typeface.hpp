// Text drawing, owned in one place.
//
// iiSU's default typeface is Cal Sans (pp4.a). raylib has no way to route DrawText through a
// custom face, so labels are drawn glyph by glyph from a loaded Font. Sizes are Android's: the
// em in pixels, with letter spacing in ems, so sizes recovered from iiSU apply unchanged.
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

namespace opensu::ui {

/// A text size as Android takes it.
struct TextStyle {
    /// The em, in pixels.
    float size{};
    /// Letter spacing, in ems, added half before and half after each glyph.
    float tracking{};
};

/// Owns the shell's typefaces.
class Typeface {
  public:
    Typeface();
    ~Typeface();

    Typeface(const Typeface&) = delete;
    Typeface& operator=(const Typeface&) = delete;

    /// The face in use, which is raylib's built-in one when no file could be
    /// loaded.
    [[nodiscard]] bool usingCustomFace() const noexcept {
        return custom_;
    }

    /// The height of the face's line box (ascent to descent) at a style.
    [[nodiscard]] float lineBox(const TextStyle& style) const noexcept;
    /// The em whose line box is `height` pixels tall.
    [[nodiscard]] float emForLineBox(float height) const noexcept;

    /// The width a label advances.
    [[nodiscard]] float measure(std::string_view text, const TextStyle& style);

    /// `text` cut to fit `room` pixels, ending in dots when it is cut.
    [[nodiscard]] std::string fitted(std::string text, float room, const TextStyle& style);

    /// Draws a label from `x`, its line box centred on `centreY`.
    void drawCentred(std::string_view text, float x, float centreY, const TextStyle& style,
                     Color colour);

    /// Cal Sans's capital height as a share of the em (navigation.md §5.3).
    static constexpr float capHeightPerEm = 0.70f;
    /// Draws a label from `origin.x` with the tops of its capitals at `origin.y`.
    void drawFromCapTop(std::string_view text, Vector2 origin, const TextStyle& style,
                        Color colour);

  private:
    struct Entry {
        Font font{};
        int size{};
        bool loaded{false};
    };

    /// The face loaded at a line box height, in pixels. Loaded once per size and cached.
    [[nodiscard]] Font face(int lineBoxPx);

    /// The file that was loaded, kept so per-size faces come from the same face.
    std::string path_;
    bool custom_{false};
    /// stb_truetype's line box per em for the loaded face; 1 for the built-in font.
    float lineBoxPerEm_{1.0f};
    /// The baseline's depth in the line box, as a share of it; hhea ascender over its height.
    float ascentShare_{0.8f};
    static constexpr int probeSize = 16;
    static constexpr int entries_ = 16;
    Entry* cache_{};
    /// The codepoints every per-size atlas holds.
    /// The loaded face's cmap: the atlas holds every character the face can draw.
    std::vector<int> codepoints_;
};

/// The shell's typefaces. One process-wide instance, because the font atlas is
/// GPU state that must not be duplicated per shell.
[[nodiscard]] Typeface& type();

} // namespace opensu::ui
