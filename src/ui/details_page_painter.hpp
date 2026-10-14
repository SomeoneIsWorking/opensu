// details_page_painter — draws the details page: the game's cover, title, badge and rows, and the
// button stack, over a faded ground.
#pragma once

#include "gpu2d/texture.hpp"

#include "details_page.hpp"
#include "paint.hpp"
#include "typeface.hpp"

namespace opensu::ui {

/// The textures the page draws, either of which may be absent.
struct DetailsArt {
    /// The game's cover.
    const gpu2d::Texture* cover{nullptr};
    /// A wide picture for the page's backdrop, when the store provides one.
    const gpu2d::Texture* hero{nullptr};

    /// The art for a tile's portrait and wide pictures, either of which may be null: the portrait
    /// is the cover, else the wide one is; the wide one is a backdrop only beside a portrait.
    [[nodiscard]] static DetailsArt of(const gpu2d::Texture* portrait,
                                       const gpu2d::Texture* wide) noexcept {
        return DetailsArt{portrait != nullptr ? portrait : wide,
                          portrait != nullptr ? wide : nullptr};
    }
};

class DetailsPagePainter {
  public:
    explicit DetailsPagePainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Draws `page` in `layout` over a `size` frame at `dp` pixels per dp, at opacity `alpha`.
    void paint(Canvas& canvas, const DetailsPage& page, const DetailsLayout& layout,
               const DetailsArt& art, Point size, float dp, float alpha) const;

  private:
    void paintCover(Canvas& canvas, const DetailsLayout& layout, const DetailsArt& art,
                    const DetailsView& view, float alpha) const;
    void paintText(Canvas& canvas, const DetailsLayout& layout, const DetailsView& view, float dp,
                   float alpha) const;
    void paintButtons(Canvas& canvas, const DetailsPage& page, const DetailsLayout& layout,
                      float dp, float alpha) const;

    Typeface& typeface_;
};

} // namespace opensu::ui
