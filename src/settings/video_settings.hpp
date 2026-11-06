// video_settings — how opensu-retro shows a game's frame: its aspect, integer scaling, sampling,
// filter, shader preset and the resolution the preset is drawn at, and the size of the touch
// controls over it.
// The keys and choices the setting layers keep them under, a game's values resolved from those
// layers, and their JSON: the per-launch file the shell writes and `opensu-retro --settings`
// reads, and the body of the runner's `POST /video`. A shader
// preset replaces the sampling and the filter; aspect and integer scale still place its output.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "setting_choices.hpp"
#include "setting_layers.hpp"
#include "setting_schema.hpp"

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

/// The resolution a shader preset's chain is drawn at.
enum class ShaderResolution : std::uint8_t {
    /// Lower while the GPU cannot keep up, full again once it can.
    Auto,
    Full,
};

inline constexpr std::string_view touchSizeKey = "touch.size";
inline constexpr int touchSizeSmallest = 75;
inline constexpr int touchSizeLargest = 175;
inline constexpr int touchSizeDefault = 100;

struct VideoSettings {
    Aspect aspect{Aspect::Core};
    IntegerScale integer{IntegerScale::Off};
    Sampling sampling{Sampling::Nearest};
    Filter filter{Filter::None};
    /// A slang preset (`.slangp`) run in place of the sampling and filter; empty for none.
    std::filesystem::path shaderPreset;
    ShaderResolution shaderResolution{ShaderResolution::Auto};
    /// The touch controls' size, a percentage of their own.
    int touchSize{touchSizeDefault};

    bool operator==(const VideoSettings&) const = default;
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

inline constexpr std::array<ChoiceText, 2> shaderResolutionChoices{{
    {"auto", "Auto"},
    {"full", "Full"},
}};

/// The video settings, each with its choices in the order of its enum.
inline constexpr std::array<ChoiceKey, 5> videoKeys{{
    {"video.aspect", "Aspect", aspectChoices},
    {"video.integer", "Integer scale", integerChoices},
    {"video.sampling", "Sampling", samplingChoices},
    {"video.filter", "Filter", filterChoices},
    {"video.shader-resolution", "Shader resolution", shaderResolutionChoices},
}};

/// The shader preset: a `.slangp` path relative to the shader pack, or "" for none at this layer.
inline constexpr std::string_view shaderKey = "video.shader";
inline constexpr std::string_view shaderLabel = "Shader preset";

/// The heading the video settings stand under.
inline constexpr std::string_view videoSection = "Video";

/// Declares the video settings in `schema`: Aspect, Integer scale, Sampling and Filter (both
/// hidden while a shader preset applies, which replaces them), the shader preset, then Shader
/// resolution (shown only while a preset applies).
void declareVideo(Schema& schema);

/// `keys`' video settings, each from its winning layer; a kept value that is no choice is
/// reported and the default used. The shader preset is found under `presets`, the shader pack.
/// The controls' size where `keys` look; the default for a value out of the range.
[[nodiscard]] int resolveTouchSize(const SettingLayers& layers, const LayerKeys& keys);

[[nodiscard]] VideoSettings resolveVideo(const SettingLayers& layers, const LayerKeys& keys,
                                         const std::filesystem::path& presets);
/// The preset `keys` resolve to, relative to the shader pack, with its layer; "" for none.
[[nodiscard]] Resolved resolveShader(const SettingLayers& layers, const LayerKeys& keys);

/// `video` as one line of JSON: the per-launch file's text and the body of the runner's
/// `POST /video`.
[[nodiscard]] std::string videoJson(const VideoSettings& video);
/// The settings in such JSON; nothing with `error` when it is not that. A key it lacks keeps its
/// default.
[[nodiscard]] std::optional<VideoSettings> parseVideoJson(std::string_view text,
                                                          std::string& error);

} // namespace opensu::settings
