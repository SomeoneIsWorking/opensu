#include "section_view.hpp"

#include <algorithm>

namespace opensu::ui {

Presentation presentationOf(library::Section section, library::LibraryMode mode) noexcept {
    if (section == library::Section::Home) {
        return Presentation::Grid;
    }
    switch (mode) {
    case library::LibraryMode::Standard:
        return Presentation::Grid;
    case library::LibraryMode::Xmb:
        return Presentation::Xmb;
    case library::LibraryMode::Carousel:
        return Presentation::Carousel;
    }
    return Presentation::Grid;
}

std::size_t visibleTiles(library::Section section, library::LibraryMode mode,
                         std::size_t items) noexcept {
    if (presentationOf(section, mode) != Presentation::Grid) {
        return std::min(items, 2 * railNeighbours + 1);
    }
    return std::min(items, static_cast<std::size_t>(gridViewport.rows * gridViewport.columns));
}

} // namespace opensu::ui
