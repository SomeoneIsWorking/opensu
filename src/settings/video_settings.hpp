// video_settings — how opensu-retro shows a game's frame: its aspect, integer scaling, sampling,
// filter, shader preset and the resolution the preset is drawn at, whether the player rotated the
// screen, the size of the touch controls over it, and the art framed around the picture per
// orientation, which the shell fills in from a visual preset (`library/visual_presets`).
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

/// When the touch controls show.
enum class TouchShow : std::uint8_t {
    /// While the latest input was not a controller's: a pad press hides them, a touch brings them
    /// back.
    Dynamic,
    Always,
    Never,
};

/// Art drawn around the picture: an RGBA image and where in its pixels the picture stands.
struct FrameArt {
    std::filesystem::path image;
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    bool operator==(const FrameArt&) const = default;
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
    TouchShow touchShow{TouchShow::Dynamic};
    /// The touch controls' size, a percentage of their own.
    int touchSize{touchSizeDefault};
    /// Rotate screen is on; the runner's `ScreenTurn` carries it out.
    bool turned{false};
    /// The frame around the picture on a screen wider than tall, and on one taller than wide.
    std::optional<FrameArt> landscapeArt{};
    std::optional<FrameArt> portraitArt{};

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

inline constexpr std::array<ChoiceText, 3> touchShowChoices{{
    {"dynamic", "Dynamic"},
    {"always", "Always"},
    {"never", "Never"},
}};

inline constexpr std::array<ChoiceText, 2> turnedChoices{{
    {"off", "Off"},
    {"on", "On"},
}};

/// The video settings, each with its choices in the order of its enum.
inline constexpr std::array<ChoiceKey, 7> videoKeys{{
    {"video.aspect", "Aspect", aspectChoices},
    {"video.integer", "Integer scale", integerChoices},
    {"video.sampling", "Sampling", samplingChoices},
    {"video.filter", "Filter", filterChoices},
    {"video.shader-resolution", "Shader resolution", shaderResolutionChoices},
    {"video.rotate", "Rotate screen", turnedChoices},
    {"touch.show", "Touch controls", touchShowChoices},
}};

/// The shader preset: a `.slangp` path relative to the shader pack, or "" for none at this layer.
inline constexpr std::string_view shaderKey = "video.shader";
inline constexpr std::string_view shaderLabel = "Shader preset";

inline constexpr std::array<ChoiceText, 3> presetChoices{{
    {"none", "None"},
    {"auto-console", "Auto Console"},
    {"crt", "CRT"},
}};
/// The visual preset: one choice for the shader, filter, aspect and frame art together. Auto
/// Console picks the game's console's own; the others are entries of the preset catalog.
inline constexpr ChoiceKey presetKey{"video.preset", "Visual preset", presetChoices};
/// What CC BY 4.0 asks to be shown where the presets' art is chosen and downloaded.
inline constexpr std::string_view presetArtCredit =
    "frames from libretro common-overlays, CC BY 4.0";
inline constexpr std::string_view noPreset = "none";
inline constexpr std::string_view autoConsolePreset = "auto-console";

/// The heading the video settings stand under.
inline constexpr std::string_view videoSection = "Video";

/// Declares the video settings in `schema`: the visual preset, Aspect, Integer scale, Sampling and
/// Filter (Sampling and Filter hidden while a shader preset applies, which replaces them), the
/// shader preset, Shader resolution (shown only while a preset applies), Touch controls, their
/// size and Rotate screen. Aspect, Filter and the shader preset are hidden while a visual preset
/// other than None applies, which sets them.
void declareVideo(Schema& schema);

/// `keys`' video settings, each from its winning layer; a kept value that is no choice is
/// reported and the default used. The shader preset is found under `presets`, the shader pack.
/// The controls' size where `keys` look; the default for a value out of the range.
[[nodiscard]] int resolveTouchSize(const SettingLayers& layers, const LayerKeys& keys);
/// The keys Aspect and Filter are kept under, with their choices.
[[nodiscard]] const ChoiceKey& aspectKey();
[[nodiscard]] const ChoiceKey& filterKey();
/// The key Rotate screen is kept under, and its spelling for `turned`.
[[nodiscard]] const ChoiceKey& turnedKey();
[[nodiscard]] std::string turnedValue(bool turned);

[[nodiscard]] VideoSettings resolveVideo(const SettingLayers& layers, const LayerKeys& keys,
                                         const std::filesystem::path& presets);
/// The preset `keys` resolve to, relative to the shader pack, with its layer; "" for none.
[[nodiscard]] Resolved resolveShader(const SettingLayers& layers, const LayerKeys& keys);
/// The visual preset `keys` resolve to, one of `presetChoices`; None for a value that is none.
[[nodiscard]] std::string resolvePreset(const SettingLayers& layers, const LayerKeys& keys);

/// `video` as one line of JSON: the per-launch file's text and the body of the runner's
/// `POST /video`.
[[nodiscard]] std::string videoJson(const VideoSettings& video);
/// The settings in such JSON; nothing with `error` when it is not that. A key it lacks keeps its
/// default.
[[nodiscard]] std::optional<VideoSettings> parseVideoJson(std::string_view text,
                                                          std::string& error);

} // namespace opensu::settings
