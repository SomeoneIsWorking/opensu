// video_settings — how opensu-retro shows a game's frame: its aspect, integer scaling, sampling,
// filter and shader preset.
// The keys and choices the setting layers keep them under, a game's values resolved from those
// layers, and the per-launch file the shell writes and `opensu-retro --settings` reads. A shader
// preset replaces the sampling and the filter; aspect and integer scale still place its output.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "setting_layers.hpp"

namespace opensu::settings {

/// The frame's shape on screen.
enum class Aspect : std::uint8_t {
    /// What the core reports.
    Core,
    FourThree,
    /// Square pixels: the frame's width over its height.
    Square,
    /// The whole window.
    Stretch,
};

enum class IntegerScale : std::uint8_t {
    Off,
    /// The largest whole multiple that fits.
    On,
    /// The smallest whole multiple that fills, cropped at the window's edges.
    Overscale,
};

enum class Sampling : std::uint8_t {
    Nearest,
    Bilinear,
    /// Each pixel scaled up by the largest whole factor, then blended bilinearly to the size.
    SharpBilinear,
};

/// A pass over the placed frame.
enum class Filter : std::uint8_t {
    None,
    /// A dark band along the bottom of every source row.
    Scanlines,
    /// A dark band along the right and bottom of every source pixel.
    LcdGrid,
    /// crt-lottes: scanlines, bloom, curvature and a shadow mask; it does its own sampling.
    Crt,
};

struct VideoSettings {
    Aspect aspect{Aspect::Core};
    IntegerScale integer{IntegerScale::Off};
    Sampling sampling{Sampling::Nearest};
    Filter filter{Filter::None};
    /// A slang preset (`.slangp`) run in place of the sampling and filter; empty for none.
    std::filesystem::path shaderPreset;

    bool operator==(const VideoSettings&) const = default;
};

/// A choice as it is kept and as the player reads it.
struct ChoiceText {
    std::string_view key;
    std::string_view label;
};

/// One video setting: its key in the layers, its label, and its choices in the order of its enum,
/// the first being the built-in default.
struct VideoKey {
    std::string_view key;
    std::string_view label;
    std::span<const ChoiceText> choices;
};

inline constexpr std::array<ChoiceText, 4> aspectChoices{{
    {"core", "Core"},
    {"4:3", "4:3"},
    {"pixel", "Pixel 1:1"},
    {"stretch", "Stretch"},
}};
inline constexpr std::array<ChoiceText, 3> integerChoices{{
    {"off", "Off"},
    {"on", "On"},
    {"overscale", "Overscale"},
}};
inline constexpr std::array<ChoiceText, 3> samplingChoices{{
    {"nearest", "Nearest"},
    {"bilinear", "Bilinear"},
    {"sharp-bilinear", "Sharp bilinear"},
}};

inline constexpr std::array<ChoiceText, 4> filterChoices{{
    {"none", "None"},
    {"scanlines", "Scanlines"},
    {"lcd-grid", "LCD grid"},
    {"crt", "CRT"},
}};

inline constexpr std::array<VideoKey, 4> videoKeys{{
    {"video.aspect", "Aspect", aspectChoices},
    {"video.integer", "Integer scale", integerChoices},
    {"video.sampling", "Sampling", samplingChoices},
    {"video.filter", "Filter", filterChoices},
}};

/// The shader preset: a `.slangp` path relative to the shader pack, or "" for none at this layer.
inline constexpr std::string_view shaderKey = "video.shader";
inline constexpr std::string_view shaderLabel = "Shader preset";

/// Whether a shader preset takes the place of video key `key` (sampling and filter).
[[nodiscard]] bool replacedByShader(std::string_view key) noexcept;

/// The video key `key` names, or null.
[[nodiscard]] const VideoKey* videoKey(std::string_view key);
/// Where `spelling` stands among `video`'s choices, or nothing when it is none of them.
[[nodiscard]] std::optional<std::size_t> choiceIndex(const VideoKey& video,
                                                     std::string_view spelling);
/// The label of `spelling` among `video`'s choices; the spelling itself when it is none of them.
[[nodiscard]] std::string_view choiceLabel(const VideoKey& video, std::string_view spelling);

/// `keys`' video settings, each from its winning layer; a kept value that is no choice is
/// reported and the default used. The shader preset is found under `presets`, the shader pack.
[[nodiscard]] VideoSettings resolveVideo(const SettingLayers& layers, const LayerKeys& keys,
                                         const std::filesystem::path& presets);
/// The preset `keys` resolve to, relative to the shader pack, with its layer; "" for none.
[[nodiscard]] Resolved resolveShader(const SettingLayers& layers, const LayerKeys& keys);

/// The per-launch file's text for `video`.
[[nodiscard]] std::string launchFileText(const VideoSettings& video);
/// The settings in a per-launch file; nothing with `error` when it is not one. A key it lacks keeps
/// its default.
[[nodiscard]] std::optional<VideoSettings> parseLaunchFile(std::string_view text,
                                                           std::string& error);

} // namespace opensu::settings
