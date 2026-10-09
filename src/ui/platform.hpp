// platform — a console's frame colours.
//
// The frame's proportions are the sprite's and live in tile_geometry; this holds
// what differs per platform.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "library/shelf.hpp"

namespace opensu::ui {

/// A platform's stroke gradient: the reference frame's colours at its two ends.
struct StrokeGradient {
    const char* console;
    std::uint32_t from;
    std::uint32_t to;
};

/// The gradients, keyed by ES-DE system name. Defined in the
/// generated platform_stroke.cpp and declared here, so the definition and its only
/// reader cannot drift apart.
extern const StrokeGradient kStrokeGradients[];

/// The number of gradients in the table above, sentinel excluded.
[[nodiscard]] std::size_t strokeGradientCount() noexcept;

/// One platform's frame.
struct Platform {
    /// The platform key, lower-case ("steam", "psx").
    std::string key;
    /// The stroke colour at the top-left and bottom-right of the frame.
    std::uint32_t strokeFrom{0};
    std::uint32_t strokeTo{0};
};

/// The platform table, built from the compiled gradients.
class Platforms {
  public:
    Platforms();

    /// The platform for a key, matched case-insensitively. Points into this table; null
    /// when there is no such key.
    [[nodiscard]] const Platform* find(std::string_view key) const;

    /// The platform a store's titles are framed in, or null when there is none. A ROM has no
    /// single store, so it is framed in its system instead and this returns null. Points into
    /// this table.
    [[nodiscard]] const Platform* forSource(library::Source source) const;

    /// The platform frame of a shelf entry: a console's and a ROM's by their system, a store
    /// title's by its store. Null for a folder of games and for an entry with no platform identity.
    [[nodiscard]] const Platform* forItem(const library::ShelfItem& item) const;

    [[nodiscard]] std::size_t size() const noexcept {
        return platforms_.size();
    }

  private:
    std::vector<Platform> platforms_;
};

} // namespace opensu::ui