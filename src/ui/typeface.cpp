#include "typeface.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "face_metrics.hpp"
#include "lucent/log.h"

namespace iideck::ui {
namespace {

/// Candidate faces, in order. Cal Sans is iiSU's default face; the others are fallbacks for a
/// machine without it.
const char* const kFacePaths[] = {
    "CalSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
};

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

std::optional<FaceMetrics> readFaceFile(const std::string& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    std::vector<char> bytes{std::istreambuf_iterator<char>{stream},
                            std::istreambuf_iterator<char>{}};
    return readFaceMetrics(std::as_bytes(std::span{bytes}));
}

/// UTF-8 labels are drawn and measured per decoded codepoint.
template <typename Visit> void forEachCodepoint(std::string_view text, Visit visit) {
    const std::string owned{text};
    for (const char* p = owned.c_str(); *p != '\0';) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(p, &bytes);
        p += std::max(bytes, 1);
        visit(codepoint);
    }
}

} // namespace

Typeface::Typeface()
    : cache_{new Entry[static_cast<std::size_t>(entries_)]}, codepoints_{atlasCodepoints()} {
    for (const char* candidate : kFacePaths) {
        const std::string path = candidate[0] == '/'
                                     ? std::string{candidate}
                                     : config::read().assetsDir.string() + "/" + candidate;
        const std::optional<FaceMetrics> metrics = readFaceFile(path);
        if (!metrics) {
            continue;
        }
        // Loaded once to validate: a face that fails here is reported rather than
        // silently falling back at the first label.
        const Font probe = LoadFontEx(path.c_str(), probeSize, nullptr, 0);
        if (probe.texture.id != 0 && probe.glyphCount > 0) {
            UnloadFont(probe);
            path_ = path;
            custom_ = true;
            lineBoxPerEm_ = metrics->lineBoxPerEm();
            ascentShare_ = metrics->ascent / (metrics->ascent - metrics->descent);
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

float Typeface::lineBox(const TextStyle& style) const noexcept {
    return style.size * lineBoxPerEm_;
}

float Typeface::emForLineBox(float height) const noexcept {
    return height / lineBoxPerEm_;
}

Font Typeface::face(int lineBoxPx) {
    lineBoxPx = std::max(lineBoxPx, 1);
    if (!custom_) {
        return GetFontDefault();
    }
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded && entry.size == lineBoxPx) {
            return entry.font;
        }
    }
    for (Entry& entry : std::span{cache_, static_cast<std::size_t>(entries_)}) {
        if (entry.loaded) {
            continue;
        }
        // stb_truetype's pixel height is the line box, ascent to descent.
        const Font loaded = LoadFontEx(path_.c_str(), lineBoxPx, codepoints_.data(),
                                       static_cast<int>(codepoints_.size()));
        if (loaded.texture.id == 0) {
            return GetFontDefault();
        }
        entry.font = loaded;
        entry.size = lineBoxPx;
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

float Typeface::measure(std::string_view text, const TextStyle& style) {
    const float box = lineBox(style);
    const Font font = face(static_cast<int>(std::lround(box)));
    const float tracking = style.tracking * style.size;
    float width = 0.0f;
    forEachCodepoint(text, [&](int codepoint) {
        // raylib has no per-codepoint measure, so a one-element array is the way to get one
        // glyph's advance.
        width += MeasureTextCodepoints(font, &codepoint, 1, box, 0.0f).x + tracking;
    });
    return width;
}

void Typeface::drawCentred(std::string_view text, float x, float centreY, const TextStyle& style,
                           Color colour) {
    const float box = lineBox(style);
    const Font font = face(static_cast<int>(std::lround(box)));
    const float tracking = style.tracking * style.size;
    const float top = centreY - box * 0.5f;
    float pen = x + tracking * 0.5f;
    forEachCodepoint(text, [&](int codepoint) {
        DrawTextCodepoint(font, codepoint, {pen, top}, box, colour);
        pen += MeasureTextCodepoints(font, &codepoint, 1, box, 0.0f).x + tracking;
    });
}

void Typeface::drawFromCapTop(std::string_view text, Vector2 origin, const TextStyle& style,
                              Color colour) {
    const float box = lineBox(style);
    const float baseline = origin.y + capHeightPerEm * style.size;
    drawCentred(text, origin.x, baseline - box * ascentShare_ + box * 0.5f, style, colour);
}

Typeface& type() {
    static Typeface instance;
    return instance;
}

} // namespace iideck::ui
