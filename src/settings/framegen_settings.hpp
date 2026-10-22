// framegen_settings — frame generation for a game, as the setting layers keep it: off, a fixed
// ratio, or adaptive to a target rate (a number, or the display's refresh) with a maximum of
// generated frames per real frame and a tolerance; the Lossless generator's performance mode and
// flow scale. The keys and their choices, and a game's values resolved from its layers into what
// the Gamescope fork's GAMESCOPE_FRAMEGEN_* root atoms take. Every default is off.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "setting_choices.hpp"
#include "setting_layers.hpp"

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

inline constexpr std::array<ChoiceText, 3> frameGenModeChoices{{
    {"off", "Off"},
    {"fixed", "Fixed"},
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
inline constexpr std::array<ChoiceText, 8> frameGenMaxChoices{{
    {"1", "1"},
    {"2", "2"},
    {"3", "3"},
    {"4", "4"},
    {"5", "5"},
    {"6", "6"},
    {"7", "7"},
    {"8", "8"},
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
/// Lossless Scaling's slider: 25 to 100 in steps of 5.
inline constexpr std::array<ChoiceText, 16> frameGenFlowScaleChoices{{
    {"100", "100%"},
    {"95", "95%"},
    {"90", "90%"},
    {"85", "85%"},
    {"80", "80%"},
    {"75", "75%"},
    {"70", "70%"},
    {"65", "65%"},
    {"60", "60%"},
    {"55", "55%"},
    {"50", "50%"},
    {"45", "45%"},
    {"40", "40%"},
    {"35", "35%"},
    {"30", "30%"},
    {"25", "25%"},
}};

/// The frame generation keys in the order the settings show them, the mode first.
inline constexpr std::array<ChoiceKey, 7> frameGenKeys{{
    {"framegen.mode", "Mode", frameGenModeChoices, 0},
    {"framegen.ratio", "Ratio", frameGenRatioChoices, 2},
    {"framegen.target", "Target", frameGenTargetChoices, 0},
    {"framegen.max", "Most generated per frame", frameGenMaxChoices, 2},
    {"framegen.tolerance", "Tolerance", frameGenToleranceChoices, 0},
    {"framegen.performance", "Performance mode", frameGenPerformanceChoices, 0},
    {"framegen.flow-scale", "Flow scale", frameGenFlowScaleChoices, 0},
}};

/// The frame generation key `key` names, or null.
[[nodiscard]] const ChoiceKey* frameGenKey(std::string_view key);
/// Whether key `key` does anything in `mode`: none in Off, the ratio in Fixed, the target, most
/// and tolerance in Adaptive, the generator's two in both; the mode itself always.
[[nodiscard]] bool frameGenKeyUsedIn(std::string_view key, FrameGenMode mode) noexcept;
/// The mode a kept spelling names, Off for one that is none.
[[nodiscard]] FrameGenMode frameGenModeOf(std::string_view spelling) noexcept;

/// `keys`' frame generation, each value from its winning layer; a kept value that is no choice is
/// reported and the default used.
[[nodiscard]] FrameGenSettings resolveFrameGen(const SettingLayers& layers, const LayerKeys& keys);

} // namespace opensu::settings
