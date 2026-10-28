#include "interface_text.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

#include <SDL3/SDL_stdinc.h>

#include "gpu2d/gpu_error.hpp"

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

} // namespace

void InterfaceText::load(SDL_GPUDevice& device) {
    for (const char* candidate : facePaths) {
        const std::filesystem::path path =
            candidate[0] == '/' ? std::filesystem::path{candidate} : assetsDir_ / candidate;
        if (!std::filesystem::is_regular_file(path)) {
            continue;
        }
        face_ = std::make_unique<gpu2d::Text>(device, path);
        lucent::info("ui", "typeface loaded from {}", path.string());
        return;
    }
    throw gpu2d::GpuError{"no usable typeface under " + assetsDir_.string()};
}

gpu2d::Text& InterfaceText::face() {
    if (face_ == nullptr) {
        throw std::logic_error{"text drawn before the typeface was loaded"};
    }
    return *face_;
}

void InterfaceText::retire() {
    if (face_ != nullptr) {
        face_->retire();
    }
}

float InterfaceText::measure(std::string_view text, float size) {
    return face().measure(text, gpu2d::TextStyle{.size = size});
}

float InterfaceText::lineHeight(float size) {
    return face().metrics(size).lineBox();
}

void InterfaceText::draw(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft,
                         float size, gpu2d::Colour colour) {
    if (text.empty() || colour.a == 0) {
        return;
    }
    const gpu2d::Point at{std::round(topLeft.x), std::round(topLeft.y)};
    if (!face().draw(canvas, text, at, gpu2d::TextStyle{.size = size}, colour)) {
        lucent::error("ui", "could not shape \"{}\": {}", text, SDL_GetError());
    }
}

void InterfaceText::drawOutline(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft,
                                float size, float outline, gpu2d::Colour colour) {
    if (text.empty() || colour.a == 0) {
        return;
    }
    const gpu2d::Point at{std::round(topLeft.x), std::round(topLeft.y)};
    if (!face().draw(canvas, text, at, gpu2d::TextStyle{.size = size, .outline = outline},
                     colour)) {
        lucent::error("ui", "could not shape \"{}\" outlined: {}", text, SDL_GetError());
    }
}

void InterfaceText::drawInkCentred(gpu2d::Canvas& canvas, std::string_view character,
                                   gpu2d::Point centre, float size, gpu2d::Colour colour) {
    if (character.empty()) {
        return;
    }
    const char* cursor = character.data();
    std::size_t left = character.size();
    const auto codepoint = static_cast<char32_t>(SDL_StepUTF8(&cursor, &left));
    const gpu2d::GlyphInk ink = face().ink(codepoint, gpu2d::TextStyle{.size = size});
    draw(canvas, character.substr(0, character.size() - left),
         gpu2d::Point{centre.x - ink.centreX(),
                      centre.y + ink.centreY() - face().metrics(size).ascent},
         size, colour);
}

void InterfaceText::drawFromCapTop(gpu2d::Canvas& canvas, std::string_view text,
                                   gpu2d::Point origin, float size, gpu2d::Colour colour) {
    const float baseline = origin.y + capHeightPerEm * size;
    draw(canvas, text, gpu2d::Point{origin.x, baseline - face().metrics(size).ascent}, size,
         colour);
}

} // namespace opensu::ui
