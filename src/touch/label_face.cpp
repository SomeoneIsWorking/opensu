#include "touch/label_face.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_surface.h>

#include "lucent/log.h"
#include "touch/touch_art.hpp"

namespace opensu::touch {
namespace {

/// Below this a word is not read, however narrow its button.
constexpr float smallestSize = 7.0F;
/// The outline, as a share of the type's size.
constexpr float outlineShare = 0.09F;

struct SurfaceFree {
    void operator()(SDL_Surface* surface) const noexcept {
        SDL_DestroySurface(surface);
    }
};
using Surface = std::unique_ptr<SDL_Surface, SurfaceFree>;

std::vector<std::string_view> wordsOf(std::string_view text) {
    std::vector<std::string_view> words;
    while (!text.empty()) {
        const std::size_t space = text.find(' ');
        if (space != 0) {
            words.push_back(text.substr(0, space));
        }
        text.remove_prefix(space == std::string_view::npos ? text.size() : space + 1);
    }
    return words;
}

/// `word` in `font` as RGBA; empty when it cannot be set.
Art lineOf(TTF_Font& font, std::string_view word, SDL_Color colour) {
    const Surface drawn{TTF_RenderText_Blended(&font, word.data(), word.size(), colour)};
    if (drawn == nullptr) {
        return {};
    }
    const Surface rgba{SDL_ConvertSurface(drawn.get(), SDL_PIXELFORMAT_RGBA32)};
    if (rgba == nullptr) {
        return {};
    }
    Art line;
    line.width = rgba->w;
    line.height = rgba->h;
    line.rgba.resize(static_cast<std::size_t>(rgba->w) * static_cast<std::size_t>(rgba->h) * 4);
    const auto* source = static_cast<const unsigned char*>(rgba->pixels);
    for (int row = 0; row < rgba->h; ++row) {
        std::copy_n(source + (static_cast<std::ptrdiff_t>(row) * rgba->pitch),
                    static_cast<std::ptrdiff_t>(rgba->w) * 4,
                    line.rgba.begin() + (static_cast<std::ptrdiff_t>(row) * rgba->w * 4));
    }
    return line;
}

} // namespace

LabelFace::LabelFace(const std::filesystem::path& font) : ready_{TTF_Init()} {
    if (!ready_) {
        lucent::warn("touch", "no labels: {}", SDL_GetError());
        return;
    }
    font_.reset(TTF_OpenFont(font.string().c_str(), smallestSize));
    if (font_ == nullptr) {
        lucent::warn("touch", "no labels: {}", SDL_GetError());
    }
}

LabelFace::~LabelFace() {
    font_.reset();
    if (ready_) {
        TTF_Quit();
    }
}

Art LabelFace::label(std::string_view text, float size, float widest) {
    const std::vector<std::string_view> words = wordsOf(text);
    if (font_ == nullptr || words.empty()) {
        return {};
    }
    TTF_Font& font = *font_;
    float fitted = std::max(size, smallestSize);
    TTF_SetFontOutline(&font, 0);
    TTF_SetFontSize(&font, fitted);
    for (const std::string_view word : words) {
        int width = 0;
        int height = 0;
        if (TTF_GetStringSize(&font, word.data(), word.size(), &width, &height) &&
            static_cast<float>(width) > widest) {
            fitted = std::max(fitted * widest / static_cast<float>(width), smallestSize);
            TTF_SetFontSize(&font, fitted);
        }
    }
    const int outline = std::max(1, static_cast<int>(std::lround(fitted * outlineShare)));
    std::vector<std::pair<Art, Art>> lines;
    int width = 0;
    int height = 0;
    for (const std::string_view word : words) {
        TTF_SetFontOutline(&font, outline);
        Art dark = lineOf(font, word, SDL_Color{10, 10, 14, 255});
        TTF_SetFontOutline(&font, 0);
        Art light = lineOf(font, word, SDL_Color{255, 255, 255, 255});
        width = std::max(width, dark.width);
        height += std::max(0, dark.height - outline);
        lines.emplace_back(std::move(dark), std::move(light));
    }
    height += outline;
    Art set;
    set.width = width;
    set.height = height;
    set.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    int top = 0;
    for (const auto& [dark, light] : lines) {
        const int left = (width - dark.width) / 2;
        composeOver(set.rgba, width, height, dark, ArtSpot{.left = left, .top = top});
        composeOver(set.rgba, width, height, light,
                    ArtSpot{.left = left + outline, .top = top + outline});
        top += std::max(0, dark.height - outline);
    }
    return set;
}

} // namespace opensu::touch
