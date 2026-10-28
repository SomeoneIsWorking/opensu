// scaling_settings — a game's render resolution and upscaling, as the setting layers keep it, the
// Steam Deck's per-game "Game resolution" and "Scaling filter". `scaling.resolution` is a share of
// the screen that keeps its shape (the screen's own, or 5/6, 3/4, 2/3, 1/2 of it, read as the size
// it comes to on the screen); `scaling.filter` how Gamescope fills the screen from it (Linear, FSR,
// NIS, Nearest, Integer); `scaling.sharpness` FSR's and NIS's sharpening. The keys, their schema
// declarations, a game's values resolved into what Gamescope's root atoms take, and the one-line
// summary the editor shows. Every default leaves Gamescope as it starts: full size, linear.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "setting_choices.hpp"
#include "setting_layers.hpp"
#include "setting_schema.hpp"

namespace opensu::settings {

/// The screen Gamescope draws on, in pixels.
struct ScreenSize {
    int width{0};
    int height{0};

    bool operator==(const ScreenSize&) const = default;
};

/// Gamescope's upscale filters as the player picks them; Integer is Nearest in whole steps.
enum class ScalingFilter : std::uint8_t { Linear, Fsr, Nis, Nearest, Integer };

/// One game's resolution and upscaling.
struct ScalingSettings {
    /// The share of the screen's size the game sees, as a fraction; 1/1 is the screen's own.
    std::uint32_t numerator{1};
    std::uint32_t denominator{1};
    ScalingFilter filter{ScalingFilter::Linear};
    /// FSR's and NIS's sharpening, 0 to 100 in steps of 5.
    std::uint32_t sharpnessPercent{90};

    bool operator==(const ScalingSettings&) const = default;

    /// Whether the game sees a smaller screen.
    [[nodiscard]] bool reduced() const noexcept {
        return numerator != denominator;
    }
    /// The size the game sees on `screen`, each side rounded to an even number of pixels.
    [[nodiscard]] ScreenSize renderSize(ScreenSize screen) const noexcept;
    /// Gamescope's sharpness, 0 sharpest to 20 softest.
    [[nodiscard]] std::uint32_t gamescopeSharpness() const noexcept {
        return 20 - (sharpnessPercent / 5);
    }
};

inline constexpr std::array<ChoiceText, 5> scalingFilterChoices{{
    {"linear", "Linear"},
    {"fsr", "AMD FSR"},
    {"nis", "NVIDIA Image Scaling"},
    {"nearest", "Nearest"},
    {"integer", "Integer"},
}};
inline constexpr ChoiceKey scalingFilterKey{"scaling.filter", "Upscaling", scalingFilterChoices, 0};
inline constexpr std::string_view scalingResolutionKey = "scaling.resolution";
/// The kept spelling of the screen's own size.
inline constexpr std::string_view scalingFullSize = "full";
/// FSR and NIS sharpening: 0 to 100 percent in steps of 5, Gamescope's 20 steps.
inline constexpr std::string_view scalingSharpnessKey = "scaling.sharpness";
inline constexpr Setting::Range scalingSharpnessRange{0, 100, 5};
inline constexpr int scalingSharpnessDefault = 90;
/// The heading the scaling settings stand under.
inline constexpr std::string_view scalingSection = "Resolution and upscaling";

/// Declares the scaling settings in `schema`; `screen` names each resolution by the size it comes
/// to, else by its share of the screen.
void declareScaling(Schema& schema, std::optional<ScreenSize> screen);

/// `keys`' scaling, each value from its winning layer; a kept value that is no choice is reported
/// and the default used.
[[nodiscard]] ScalingSettings resolveScaling(const SettingLayers& layers, const LayerKeys& keys);
/// The same from the values `applies` reads (a scope's in the editor); `where` names them in a
/// report.
[[nodiscard]] ScalingSettings resolveScaling(const SettingLookup& applies, std::string_view where);

/// What `settings` does in one line: "Full size · Linear", "1280 x 720 · AMD FSR · sharpness 90%".
[[nodiscard]] std::string scalingSummary(const ScalingSettings& settings,
                                         std::optional<ScreenSize> screen);

} // namespace opensu::settings
