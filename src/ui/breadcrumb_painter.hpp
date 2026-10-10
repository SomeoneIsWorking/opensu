// breadcrumb_painter — draws the breadcrumb trail as a glass pill with a chevron between levels.
#pragma once

#include <optional>

#include "breadcrumbs.hpp"
#include "glass.hpp"
#include "top_bar_metrics.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class BreadcrumbPainter {
  public:
    explicit BreadcrumbPainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Where `trail` stands in `frame`: each label measured at the title pill's text size.
    [[nodiscard]] BreadcrumbLayout layout(const Trail& trail, const BreadcrumbFrame& frame,
                                          const TitlePillMetrics& text) const;

    /// Draws `trail` at `layout`. `hovered` is the level under the pointer.
    void paint(const Trail& trail, const BreadcrumbLayout& layout, const TitlePillMetrics& text,
               float dp, std::optional<std::size_t> hovered) const;

  private:
    Typeface& typeface_;
    GlassPainter glass_;
};

} // namespace opensu::ui
