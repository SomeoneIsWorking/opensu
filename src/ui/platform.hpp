// platform — a console's identity: its border sprite, its logo, and its tint.
//
// The reference gives every platform a 1024x1024 border sprite, a small logo,
// and an HSL tint. Measured from the shipped pack rather than guessed:
//
//   - the stroke is 26px on a 1024 canvas, so 2.54% of the tile's width
//   - the corner tab is 180x180, the same 6.25% the pack declares as the radius,
//     and it hangs off the top-left
//   - the stroke is a diagonal gradient between two colours, one per platform:
//     Steam runs #0b5bb3 to #33a1d7, 3DO runs red to orange
//
// The artwork and the border are the same 1024x1024 shape, which is how a box art
// and its frame stay aligned when one is composited onto the other.
#pragma once

#include <cstdint>
#include <vector>

#include "library/game.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace iideck::ui {

/// A platform's stroke gradient, as recovered from the reference's border pack.
struct StrokeGradient {
    const char* console;
    std::uint32_t from;
    std::uint32_t to;
};

/// The pack's gradients, keyed by the pack's own console names. Defined in the
/// generated platform_stroke.cpp and declared here, so the definition and its only
/// reader cannot drift apart.
extern const StrokeGradient kStrokeGradients[];

/// The number of gradients in the table above, sentinel excluded.
[[nodiscard]] std::size_t strokeGradientCount() noexcept;

/// One platform's identity, as the reference defines it.
struct Platform {
    /// The platform key, matching the reference's console names ("steam", "psx").
    std::string key;
    /// The border sprite's path, if the platform has one.
    std::filesystem::path border;
    /// The logo's path, if the platform has one.
    std::filesystem::path logo;

    /// The stroke colour at the top-left and bottom-right of the frame, as the
    /// gradient's endpoints. Recovered from the sprite.
    std::uint32_t strokeFrom{0};
    std::uint32_t strokeTo{0};
    /// The stroke's thickness as a fraction of the border canvas's width.
    float strokeFraction{0.0254f};
    /// The corner tab's size as a fraction of the border canvas's width.
    float tabFraction{0.1758f};

    /// The tint applied to this platform's artwork, as additive HSL offsets.
    /// Hue is in degrees and wraps; saturation and lightness are additive.
    struct Tint {
        float hueDegrees{0.0f};
        float saturation{0.0f};
        float lightness{0.0f};

        [[nodiscard]] bool identity() const noexcept {
            return hueDegrees == 0.0f && saturation == 0.0f && lightness == 0.0f;
        }
    };
    Tint tint{};

    [[nodiscard]] bool hasBorder() const noexcept {
        return !border.empty();
    }
};

/// The platform table, read once from the reference's own border pack.
class Platforms {
  public:
    /// Reads the pack at `root`, which is a directory holding `border_pack.json`.
    /// A missing pack is not an error: every platform simply has no border, which
    /// is what the shell drew before. The gradients come from the pack's sprites
    /// and are always available, since they are compiled in.
    static Platforms load(const std::filesystem::path& root);

    /// The platform for a key, matched case-insensitively because the pack is
    /// inconsistent about it ("steam" and "PC" and "windows" all appear). Points
    /// into this table; null when the pack has no such key.
    [[nodiscard]] const Platform* find(std::string_view key) const;

    /// The platform a store's titles are framed in, or null when the pack has no
    /// border for it. A ROM has no single store, so it is framed in its system
    /// instead and this returns null.
    ///
    /// The pointer is into the table this owns and stays valid for the table's
    /// lifetime. Returning a pointer into a temporary would dangle.
    [[nodiscard]] const Platform* forSource(library::Source source) const;

    [[nodiscard]] std::size_t size() const noexcept {
        return platforms_.size();
    }

  private:
    std::vector<Platform> platforms_;
};

/// The default pack location, beside the typeface in `assets/`.
[[nodiscard]] std::filesystem::path defaultPlatformRoot();

} // namespace iideck::ui