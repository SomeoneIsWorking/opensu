// How each section is presented, and how many tiles it shows at once.
#include "section_view.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using iideck::library::LibraryMode;
using iideck::library::Section;
using iideck::test::expect;
using iideck::ui::Presentation;

void viewports() {
    expect(iideck::ui::gridViewport.rows == 3 && iideck::ui::gridViewport.columns == 4,
           "the grid is 3 rows by 4 columns, in Home and in Library's Standard");
}

void presentations() {
    for (const LibraryMode mode : iideck::library::allLibraryModes) {
        expect(iideck::ui::presentationOf(Section::Home, mode) == Presentation::Grid,
               "Home is a grid whatever Library's mode");
    }
    expect(iideck::ui::presentationOf(Section::Library, LibraryMode::Standard) ==
               Presentation::Grid,
           "Standard is the grid");
    expect(iideck::ui::presentationOf(Section::Library, LibraryMode::Xmb) == Presentation::Xmb,
           "XMB is the column");
    expect(iideck::ui::presentationOf(Section::Library, LibraryMode::Carousel) ==
               Presentation::Carousel,
           "Carousel is the row");
}

void visible() {
    const auto count = [](Section section, LibraryMode mode, std::size_t items) {
        return iideck::ui::visibleTiles(section, mode, items);
    };
    expect(count(Section::Home, LibraryMode::Standard, 30) == 12, "Home shows 3 x 4 tiles");
    expect(count(Section::Home, LibraryMode::Xmb, 30) == 12, "whatever Library's mode");
    expect(count(Section::Home, LibraryMode::Standard, 5) == 5, "or all it has");
    expect(count(Section::Home, LibraryMode::Standard, 0) == 0, "or none");
    expect(count(Section::Library, LibraryMode::Standard, 30) == 12, "Library's grid shows 3 x 4");
    expect(count(Section::Library, LibraryMode::Standard, 6) == 6, "or all it has");
    expect(count(Section::Library, LibraryMode::Xmb, 30) == 3,
           "an XMB shows the focus and one each side");
    expect(count(Section::Library, LibraryMode::Carousel, 2) == 2, "or all it has");
}

} // namespace

int main() {
    viewports();
    presentations();
    visible();
    std::printf("section_view: all checks passed\n");
    return 0;
}
