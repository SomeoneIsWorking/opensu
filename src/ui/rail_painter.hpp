// rail_painter — what an XMB and a Carousel draw besides their tiles: the XMB's left column (the
// section icon in the focused console's colours, or the console's own card inside it, and the
// marker pointing at the focused tile), the focused tile's title, and the Carousel's marker under
// a focused game (navigation.md §5.3, §5.4).
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "raylib.h"

#include "library/shelf.hpp"
#include "platform.hpp"
#include "rail_layout.hpp"

namespace iideck::ui {

/// How the rail's extras are inked: the theme, and how far they have faded in.
struct RailStyle {
    bool dark{false};
    float alpha{1.0f};
};

/// What the left column's icon takes its colours from.
struct Focused {
    /// A key for the colours: the console's system, or empty.
    std::string key;
    /// The console's card on disk, whose border gives them, or empty.
    std::filesystem::path card;
    /// The platform whose stroke gives them when there is no card, or null.
    const Platform* platform{nullptr};
};

class RailPainter {
  public:
    RailPainter() = default;
    ~RailPainter();
    RailPainter(const RailPainter&) = delete;
    RailPainter& operator=(const RailPainter&) = delete;

    /// The gamepad icon's file the left column draws; replaces the textures made from the last.
    void setSectionIcon(const std::filesystem::path& file);

    /// The XMB's left column at a console list: the icon in the colours of the focused tile's
    /// border (a console's own card, else its platform's stroke, else the icon's own) with the
    /// marker beside it.
    void paintColumn(const XmbLayout& layout, const Focused& focused, const RailStyle& style);

    /// The marker alone, for the column inside a console, where a card stands in the icon's place.
    void paintColumnMarker(const XmbLayout& layout, const RailStyle& style) const;

    /// The focused tile's title, right of it in an XMB or centred over a Carousel.
    void paintTitle(const XmbLayout& layout, std::string_view title, const RailStyle& style);
    void paintTitle(const CarouselLayout& layout, std::string_view title, const RailStyle& style);

    /// The marker under the Carousel's focused game.
    void paintRowMarker(const CarouselLayout& layout, const RailStyle& style) const;

  private:
    /// The icon recoloured for `focused`, made on first use; null while there is no icon file.
    const Texture* tinted(const Focused& focused);
    void unloadTints();
    /// Where a title stands: its x (left edge, or centre when `centred`), its capitals' top and
    /// height, and the dp the shadow is sized by.
    struct TitleBox {
        float x;
        float capTop;
        float capHeight;
        float dp;
        bool centred;
    };
    void drawTitle(std::string_view title, const TitleBox& box, const RailStyle& style);

    std::filesystem::path sectionIcon_;
    std::map<std::string, Texture, std::less<>> tints_;
};

} // namespace iideck::ui
