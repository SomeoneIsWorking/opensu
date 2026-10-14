#include "typeface.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gpu2d/gpu_error.hpp"

#include "face_metrics.hpp"
#include "lucent/log.h"

namespace opensu::ui {
namespace {

/// Candidate faces, in order. Cal Sans is iiSU's default face; the others are fallbacks for a
/// machine without it.
constexpr std::array<const char*, 3> facePaths{
    "CalSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
};

/// stb_truetype drew the outlines unhinted and placed glyphs by advance alone.
constexpr gpu2d::FaceOptions faceOptions{.hinted = false, .kerned = false};

std::optional<FaceMetrics> readFaceFile(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
    return readFaceMetrics(std::as_bytes(std::span{bytes}));
}

} // namespace

Typeface::Typeface(std::filesystem::path assetsDir) : assetsDir_{std::move(assetsDir)} {
}

void Typeface::load(SDL_GPUDevice& device) {
    for (const char* candidate : facePaths) {
        const std::filesystem::path path =
            candidate[0] == '/' ? std::filesystem::path{candidate} : assetsDir_ / candidate;
        std::optional<FaceMetrics> metrics = readFaceFile(path);
        if (!metrics) {
            continue;
        }
        text_ = std::make_unique<gpu2d::Text>(device, path, faceOptions);
        face_ = std::move(*metrics);
        lucent::info("ui", "typeface loaded from {}", path.string());
        return;
    }
    throw gpu2d::GpuError{"no usable typeface under " + assetsDir_.string()};
}

gpu2d::Text& Typeface::text() {
    if (text_ == nullptr) {
        throw std::logic_error{"text drawn before the typeface was loaded"};
    }
    return *text_;
}

float Typeface::lineBox(const TextStyle& style) const noexcept {
    return style.size * face_.lineBoxPerEm();
}

float Typeface::emForLineBox(float height) const noexcept {
    return height / face_.lineBoxPerEm();
}

float Typeface::measure(std::string_view text, const TextStyle& style) const noexcept {
    return TextGrid{face_, style}.measure(text);
}

std::string Typeface::fitted(std::string text, float room, const TextStyle& style) {
    if (measure(text, style) <= room) {
        return text;
    }
    while (!text.empty() && measure(text + "...", style) > room) {
        do {
            text.pop_back();
        } while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0U) == 0x80U);
    }
    return text + "...";
}

void Typeface::drawCentred(Canvas& canvas, std::string_view text, float x, float centreY,
                           const TextStyle& style, Colour colour) {
    if (text.empty() || colour.a == 0) {
        return;
    }
    gpu2d::Text& glyphs = this->text();
    const TextGrid grid{face_, style};
    const gpu2d::TextStyle glyphStyle{.size = grid.em()};
    // A one-glyph run puts its baseline SDL_ttf's own ascent below its top.
    const float baseline = centreY - grid.lineBox() * 0.5f + grid.ascent();
    const float runTop = std::round(baseline - glyphs.metrics(grid.em()).ascent);
    float pen = x + grid.lead();
    forEachCodepoint(text, [&](char32_t codepoint, std::string_view bytes) {
        if (!glyphs.draw(canvas, bytes, Point{std::round(pen), runTop}, glyphStyle, colour)) {
            lucent::error("ui", "could not shape \"{}\": {}", bytes, SDL_GetError());
        }
        pen += grid.advance(codepoint);
    });
}

void Typeface::drawFromCapTop(Canvas& canvas, std::string_view text, Point origin,
                              const TextStyle& style, Colour colour) {
    const float box = lineBox(style);
    const float baseline = origin.y + capHeightPerEm * style.size;
    const float ascentShare = face_.ascent / (face_.ascent - face_.descent);
    drawCentred(canvas, text, origin.x, baseline - box * ascentShare + box * 0.5f, style, colour);
}

void Typeface::retire() {
    if (text_ != nullptr) {
        text_->retire();
    }
}

} // namespace opensu::ui
