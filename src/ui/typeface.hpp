// Text drawing, owned in one place.
//
// The reference uses a rounded display face; raylib's built-in font is not it.
// raylib has no way to route DrawText through a custom face, so labels are drawn
// glyph by glyph from a loaded Font. Doing that here keeps every call site
// measuring and drawing through the same face.
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "raylib.h"

namespace iideck::ui {

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

    /// The face at a pixel size. Faces are loaded once per size and cached.
    [[nodiscard]] Font face(int size);

    /// The width a label occupies.
    [[nodiscard]] float measure(std::string_view text, int size);

    /// Draws a label. The parameter order matches raylib's DrawText so call
    /// sites read the same as the rest of the shell.
    void draw(const char* text, float x, float y, int size, Color colour);

  private:
    struct Entry {
        Font font{};
        int size{};
        bool loaded{false};
    };

    /// The file that was loaded, kept so per-size faces come from the same face.
    std::string path_;
    bool custom_{false};
    static constexpr int probeSize = 16;
    static constexpr int entries_ = 16;
    Entry* cache_{};
};

/// The shell's typefaces. One process-wide instance, because the font atlas is
/// GPU state that must not be duplicated per shell.
[[nodiscard]] Typeface& type();

} // namespace iideck::ui