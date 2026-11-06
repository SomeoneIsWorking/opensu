// label_face — words for the on-screen controls, set from an outline font at the size they are
// drawn, so they stay crisp at any resolution: white with a dark outline, a word to a line.
#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

#include <SDL3_ttf/SDL_ttf.h>

#include "touch/touch_controls.hpp"

namespace opensu::touch {

class LabelFace {
  public:
    /// Sets words in `font`; none are drawn, the reason logged, when it cannot be opened.
    explicit LabelFace(const std::filesystem::path& font);
    LabelFace(const LabelFace&) = delete;
    LabelFace& operator=(const LabelFace&) = delete;
    LabelFace(LabelFace&&) = delete;
    LabelFace& operator=(LabelFace&&) = delete;
    ~LabelFace();

    /// `text` at `size` pixels, smaller where a word would pass `widest`; empty without a font.
    [[nodiscard]] Art label(std::string_view text, float size, float widest);

  private:
    struct FontClose {
        void operator()(TTF_Font* font) const noexcept {
            TTF_CloseFont(font);
        }
    };

    bool ready_{false};
    std::unique_ptr<TTF_Font, FontClose> font_;
};

} // namespace opensu::touch
