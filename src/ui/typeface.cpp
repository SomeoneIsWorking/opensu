#include "typeface.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "lucent/log.h"

namespace iideck::ui {
namespace {

/// Candidate faces, in order. Nunito is the reference's own typeface; the others
/// are fallbacks for a machine without it. Only TrueType outlines load here, so a
/// rounded CFF face such as Comfortaa cannot be used.
const char* const kFacePaths[] = {
    "Nunito-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
};

/// Extra spacing between glyphs, in pixels at the face's own scale.
constexpr float spacing = 0.0f;

/// Printable ASCII, Latin-1 and the typographic punctuation labels use; raylib's default
/// atlas stops at 126, so a "·" drew as "?".
std::vector<int> atlasCodepoints() {
    std::vector<int> codepoints;
    for (int c = 0x20; c <= 0x7E; ++c) {
        codepoints.push_back(c);
    }
    for (int c = 0xA0; c <= 0xFF; ++c) {
        codepoints.push_back(c);
    }
    for (const int c : {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}) {
        codepoints.push_back(c);
    }
    return codepoints;
}

} // namespace

Typeface::Typeface()
    : cache_{new Entry[static_cast<std::size_t>(entries_)]}, codepoints_{atlasCodepoints()} {
    for (const char* candidate : kFacePaths) {
        const std::string path = candidate[0] == '/'
                                     ? std::string{candidate}
                                     : config::read().assetsDir.string() + "/" + candidate;
        // Loaded once to validate: a face that fails here is reported rather than
        // silently falling back at the first label.
        const Font probe = LoadFontEx(path.c_str(), probeSize, nullptr, 0);
        if (probe.texture.id != 0 && probe.glyphCount > 0) {
            UnloadFont(probe);
            path_ = path;
            custom_ = true;
            lucent::info("ui", "typeface loaded from {}", path);
            return;
        }
        if (probe.texture.id != 0) {
            UnloadFont(probe);
        }
    }
    lucent::warn("ui", "no usable typeface found; falling back to the built-in font");
}

Typeface::~Typeface() {
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded) {
            UnloadFont(entry.font);
        }
    }
    delete[] cache_;
}

Font Typeface::face(int size) {
    size = std::max(size, 1);
    if (!custom_) {
        return GetFontDefault();
    }
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded && entry.size == size) {
            return entry.font;
        }
    }
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded) {
            continue;
        }
        const Font loaded = LoadFontEx(path_.c_str(), size, codepoints_.data(),
                                       static_cast<int>(codepoints_.size()));
        if (loaded.texture.id == 0) {
            return GetFontDefault();
        }
        entry.font = loaded;
        entry.size = size;
        entry.loaded = true;
        return entry.font;
    }
    // Every cache slot is taken; the smallest loaded face is a better answer than
    // the built-in font, since it at least matches the design.
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded) {
            return entry.font;
        }
    }
    return GetFontDefault();
}

float Typeface::measure(std::string_view text, int size) {
    if (!custom_) {
        return MeasureText(std::string{text}.c_str(), size);
    }
    const Font face = this->face(size);
    const std::string owned{text};
    return MeasureTextEx(face, owned.c_str(), static_cast<float>(size), spacing).x;
}

void Typeface::draw(const char* text, float x, float y, int size, Color colour) {
    if (!custom_) {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), size, colour);
        return;
    }
    const Font face = this->face(size);
    float pen = x;
    // Labels are UTF-8, so a glyph is a decoded codepoint, not a byte.
    for (const char* p = text; *p != '\0';) {
        int bytes = 0;
        const int glyph = GetCodepointNext(p, &bytes);
        p += std::max(bytes, 1);
        DrawTextCodepoint(face, glyph, {pen, y}, static_cast<float>(size), colour);
        // raylib has no per-codepoint measure, so a one-element array is the
        // way to get this glyph's advance.
        const Vector2 advance =
            MeasureTextCodepoints(face, &glyph, 1, static_cast<float>(size), spacing);
        pen += advance.x;
    }
}

Typeface& type() {
    static Typeface instance;
    return instance;
}

} // namespace iideck::ui