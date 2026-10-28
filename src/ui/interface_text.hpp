// interface_text — openSU's face (Cal Sans, hinted and kerned): what every painter and the
// interface toolkit draw words with.
#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/text.hpp"
#include "gpu2d/ui/typesetter.hpp"

namespace opensu::ui {

/// Cal Sans's capital height as a share of the em (OS/2 sCapHeight 700 of 1000).
inline constexpr float capHeightPerEm = 0.70f;

class InterfaceText final : public gpu2d::ui::Typesetter {
  public:
    /// Faces are read from `assetsDir`; nothing is loaded until `load`.
    explicit InterfaceText(std::filesystem::path assetsDir) : assetsDir_{std::move(assetsDir)} {
    }

    /// Loads the first face found onto `device`. Throws gpu2d::GpuError when none loads.
    void load(SDL_GPUDevice& device);
    /// Releases the labels the last frame did not draw. Once per frame, after it was submitted.
    void retire();

    [[nodiscard]] float measure(std::string_view text, float size) override;
    [[nodiscard]] float lineHeight(float size) override;
    void draw(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft, float size,
              gpu2d::Colour colour) override;
    void drawOutline(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft, float size,
                     float outline, gpu2d::Colour colour) override;

    /// Draws the first character of `character` with the middle of its ink on `centre`.
    void drawInkCentred(gpu2d::Canvas& canvas, std::string_view character, gpu2d::Point centre,
                        float size, gpu2d::Colour colour);
    /// Draws `text` from `origin.x` with the tops of its capitals at `origin.y`.
    void drawFromCapTop(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point origin,
                        float size, gpu2d::Colour colour);

  private:
    [[nodiscard]] gpu2d::Text& face();

    std::filesystem::path assetsDir_;
    std::unique_ptr<gpu2d::Text> face_;
};

} // namespace opensu::ui
