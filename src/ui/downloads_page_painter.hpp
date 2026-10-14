// downloads_page_painter — draws the Downloads page: the card of rows, each with its title, source,
// state and figures, a progress bar and its buttons, or "No downloads" on an empty card.
#pragma once

#include "paint.hpp"

#include "downloads_page.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class DownloadsPagePainter {
  public:
    explicit DownloadsPagePainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Draws `page` in `layout` over a `size` frame at `dp` pixels per dp, at opacity `alpha`.
    void paint(Canvas& canvas, const DownloadsPage& page, const DownloadsLayout& layout, Point size,
               float dp, float alpha) const;

  private:
    void paintRow(Canvas& canvas, const DownloadsEntry& entry, const DownloadsLayout& layout,
                  std::size_t index, const DownloadsPage& page, float dp, float alpha) const;

    Typeface& typeface_;
};

} // namespace opensu::ui
