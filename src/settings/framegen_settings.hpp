// framegen_settings — frame generation for a game, as the setting layers keep it. `framegen.preset`
// decides first: Off, Smooth 60 (adaptive to 60 fps), Match display (adaptive to the display's
// refresh) or Custom, which reads the detail keys: a fixed ratio or adaptive to a target rate with
// a maximum of generated frames per real frame and a tolerance, and the Lossless generator's
// performance mode and flow scale. The keys, their schema declarations, a game's values resolved
// into what the Gamescope fork's GAMESCOPE_FRAMEGEN_* root atoms take, and the one-line summary
// the editor shows. Every default is off.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "setting_choices.hpp"
#include "setting_layers.hpp"
#include "setting_schema.hpp"

namespace opensu::settings {

enum class FrameGenMode : std::uint8_t { Off, Fixed, Adaptive };

/// One game's frame generation, in the units of the fork's atoms.
struct FrameGenSettings {
    FrameGenMode mode{FrameGenMode::Off};
    /// Shown frames per real frame in Fixed, in thousandths (2000 is 2x).
    std::uint32_t ratioThousandths{2000};
    /// Adaptive's target in thousandths of fps; 0 is the display's refresh.
    std::uint32_t targetThousandths{0};
    /// Adaptive's most generated frames per real frame.
    std::uint32_t maxGenerated{3};
    /// Adaptive: a base rate this close to the target generates nothing, in thousandths of fps.
    std::uint32_t toleranceThousandths{0};
    /// The Lossless generator's performance size mode, else balanced.
    bool performance{false};
    /// The resolution motion is estimated at, in percent of the frame.
    std::uint32_t flowScalePercent{100};

    bool operator==(const FrameGenSettings&) const = default;
};

inline constexpr std::array<ChoiceText, 4> frameGenPresetChoices{{
    {"off", "Off"},
    {"smooth-60", "Smooth 60"},
    {"display", "Match display"},
    {"custom", "Custom"},
}};
inline constexpr std::array<ChoiceText, 2> frameGenModeChoices{{
    {"fixed", "Fixed ratio"},
    {"adaptive", "Adaptive"},
}};
inline constexpr std::array<ChoiceText, 7> frameGenRatioChoices{{
    {"1.25", "1.25x"},
    {"1.5", "1.5x"},
    {"2", "2x"},
    {"2.5", "2.5x"},
    {"3", "3x"},
    {"4", "4x"},
    {"5", "5x"},
}};
inline constexpr std::array<ChoiceText, 14> frameGenTargetChoices{{
    {"display", "Display refresh"},
    {"30", "30 fps"},
    {"40", "40 fps"},
    {"45", "45 fps"},
    {"50", "50 fps"},
    {"60", "60 fps"},
    {"72", "72 fps"},
    {"75", "75 fps"},
    {"90", "90 fps"},
    {"100", "100 fps"},
    {"120", "120 fps"},
    {"144", "144 fps"},
    {"165", "165 fps"},
    {"240", "240 fps"},
}};
/// Kept as generated frames per real frame; read as the most frames shown per real one.
inline constexpr std::array<ChoiceText, 8> frameGenMaxChoices{{
    {"1", "2x"},
    {"2", "3x"},
    {"3", "4x"},
    {"4", "5x"},
    {"5", "6x"},
    {"6", "7x"},
    {"7", "8x"},
    {"8", "9x"},
}};
inline constexpr std::array<ChoiceText, 9> frameGenToleranceChoices{{
    {"0", "None"},
    {"1", "1 fps"},
    {"2", "2 fps"},
    {"3", "3 fps"},
    {"4", "4 fps"},
    {"5", "5 fps"},
    {"6", "6 fps"},
    {"8", "8 fps"},
    {"10", "10 fps"},
}};
inline constexpr std::array<ChoiceText, 2> frameGenPerformanceChoices{{
    {"off", "Off"},
    {"on", "On"},
}};

/// The frame generation keys kept as one of a list, in the order the editor shows them.
inline constexpr std::array<ChoiceKey, 7> frameGenKeys{{
    {"framegen.preset", "Frame generation", frameGenPresetChoices, 0},
    {"framegen.mode", "Pacing", frameGenModeChoices, 1},
    {"framegen.ratio", "Ratio", frameGenRatioChoices, 2},
    {"framegen.target", "Target", frameGenTargetChoices, 0},
    {"framegen.max", "Most frames per real one", frameGenMaxChoices, 2},
    {"framegen.tolerance", "Tolerance", frameGenToleranceChoices, 0},
    {"framegen.performance", "Performance mode", frameGenPerformanceChoices, 0},
}};
/// Lossless Scaling's flow scale slider: 25 to 100 percent in steps of 5.
inline constexpr std::string_view frameGenFlowScaleKey = "framegen.flow-scale";
inline constexpr Setting::Range frameGenFlowScaleRange{25, 100, 5};
inline constexpr int frameGenFlowScaleDefault = 100;
/// The heading frame generation stands under.
inline constexpr std::string_view frameGenSection = "Frame generation";

/// Declares frame generation's settings in `schema`: the preset, then the detail rows Custom shows.
void declareFrameGen(Schema& schema);

/// `keys`' frame generation, each value from its winning layer; a kept value that is no choice is
/// reported and the default used.
[[nodiscard]] FrameGenSettings resolveFrameGen(const SettingLayers& layers, const LayerKeys& keys);
/// The same from the values `applies` reads (a scope's in the editor); `where` names them in a
/// report.
[[nodiscard]] FrameGenSettings resolveFrameGen(const SettingLookup& applies,
                                               std::string_view where);

/// What `settings` does in one line: "Off", "Fixed ratio · 2x", "Adaptive · target 60 fps · up to
/// 3x · tolerance 5 fps".
[[nodiscard]] std::string frameGenSummary(const FrameGenSettings& settings);

/// The detail keys and values a named preset (`smooth-60`, `display`) stands for, so choosing
/// Custom over one starts from what it did; empty for another spelling.
[[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>>
frameGenPresetValues(std::string_view preset);

/// Moves layers an older openSU wrote (`framegen.mode` off, fixed or adaptive, no preset) onto the
/// preset: off becomes the Off preset, a mode becomes Custom with it. True when anything moved.
bool moveFrameGenModesToPresets(SettingLayers& layers);

} // namespace opensu::settings
