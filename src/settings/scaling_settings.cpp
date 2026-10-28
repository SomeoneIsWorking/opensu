#include "scaling_settings.hpp"

#include <algorithm>
#include <charconv>

#include "lucent/log.h"

namespace opensu::settings {
namespace {

/// A kept resolution and the share of the screen it stands for.
struct Share {
    std::string_view key;
    std::uint32_t numerator;
    std::uint32_t denominator;
};

constexpr std::array<Share, 5> shares{{
    {scalingFullSize, 1, 1},
    {"83", 5, 6},
    {"75", 3, 4},
    {"67", 2, 3},
    {"50", 1, 2},
}};

int evenOf(std::uint64_t scaled, std::uint32_t denominator) {
    // Nearest even number of pixels.
    return static_cast<int>(((scaled + denominator) / (2ULL * denominator)) * 2ULL);
}

std::string sizeText(ScreenSize size) {
    return std::to_string(size.width) + " x " + std::to_string(size.height);
}

std::string shareLabel(const Share& share, std::optional<ScreenSize> screen) {
    const ScalingSettings scaled{.numerator = share.numerator, .denominator = share.denominator};
    if (share.key == scalingFullSize) {
        return screen ? "Full: " + sizeText(*screen) : std::string{"Full"};
    }
    if (!screen) {
        return std::string{share.key} + "% of the screen";
    }
    return sizeText(scaled.renderSize(*screen));
}

std::uint32_t sharpnessOf(const std::string& kept, std::string_view where) {
    std::uint32_t percent = scalingSharpnessDefault;
    const auto [end, error] = std::from_chars(kept.data(), kept.data() + kept.size(), percent);
    if (error != std::errc{} || end != kept.data() + kept.size() ||
        percent > static_cast<std::uint32_t>(scalingSharpnessRange.high) ||
        percent % static_cast<std::uint32_t>(scalingSharpnessRange.step) != 0) {
        lucent::warn("settings", "{}: {} is not a sharpness; using {}", where, kept,
                     scalingSharpnessDefault);
        return scalingSharpnessDefault;
    }
    return percent;
}

bool sharpens(const SettingLookup& applies) {
    const std::string filter = applies(scalingFilterKey.key);
    return filter == scalingFilterChoices[1].key || filter == scalingFilterChoices[2].key;
}

} // namespace

ScreenSize ScalingSettings::renderSize(ScreenSize screen) const noexcept {
    if (!reduced()) {
        return screen;
    }
    return ScreenSize{
        evenOf(static_cast<std::uint64_t>(screen.width) * numerator, denominator),
        evenOf(static_cast<std::uint64_t>(screen.height) * numerator, denominator),
    };
}

void declareScaling(Schema& schema, std::optional<ScreenSize> screen) {
    Setting resolution;
    resolution.key = std::string{scalingResolutionKey};
    resolution.label = "Resolution";
    resolution.help =
        "Lower runs faster: the game draws fewer pixels and is upscaled to the screen";
    resolution.section = std::string{scalingSection};
    for (const Share& share : shares) {
        resolution.choices.push_back(
            SettingChoice{std::string{share.key}, shareLabel(share, screen)});
    }
    resolution.fallback = std::string{scalingFullSize};
    resolution.takes = Takes::NextLaunch;
    schema.add(std::move(resolution));

    schema.add(choiceSetting(scalingFilterKey,
                             "How the smaller picture is enlarged to fill the screen",
                             std::string{scalingSection}));

    Setting sharpness;
    sharpness.key = std::string{scalingSharpnessKey};
    sharpness.label = "Sharpness";
    sharpness.help = "How much FSR or NIS sharpens the upscaled picture";
    sharpness.section = std::string{scalingSection};
    sharpness.type = SettingType::Slider;
    sharpness.range = scalingSharpnessRange;
    sharpness.unit = "%";
    sharpness.fallback = std::to_string(scalingSharpnessDefault);
    sharpness.shownWhen = sharpens;
    schema.add(std::move(sharpness));
}

ScalingSettings resolveScaling(const SettingLayers& layers, const LayerKeys& keys) {
    return resolveScaling(
        [&layers, &keys](std::string_view key) {
            std::string fallback = std::to_string(scalingSharpnessDefault);
            if (key == scalingResolutionKey) {
                fallback = std::string{scalingFullSize};
            } else if (key == scalingFilterKey.key) {
                fallback = std::string{scalingFilterKey.defaultKey()};
            }
            return layers.resolve(keys, key, fallback).value;
        },
        keys.game);
}

ScalingSettings resolveScaling(const SettingLookup& applies, std::string_view where) {
    ScalingSettings settings;
    const std::string resolution = applies(scalingResolutionKey);
    const auto share = std::ranges::find(shares, resolution, &Share::key);
    if (share != shares.end()) {
        settings.numerator = share->numerator;
        settings.denominator = share->denominator;
    } else {
        lucent::warn("settings", "{}: {} is not a resolution; using the full size", where,
                     resolution);
    }
    settings.filter = static_cast<ScalingFilter>(
        choiceIndexOrDefault(scalingFilterKey, applies(scalingFilterKey.key), where));
    settings.sharpnessPercent = sharpnessOf(applies(scalingSharpnessKey), where);
    return settings;
}

std::string scalingSummary(const ScalingSettings& settings, std::optional<ScreenSize> screen) {
    std::string text;
    if (!settings.reduced()) {
        text = "Full size";
    } else if (screen) {
        text = sizeText(settings.renderSize(*screen));
    } else {
        text = std::to_string((100 * settings.numerator + settings.denominator / 2) /
                              settings.denominator) +
               "% of the screen";
    }
    text += " · ";
    text += scalingFilterChoices[static_cast<std::size_t>(settings.filter)].label;
    if (settings.filter == ScalingFilter::Fsr || settings.filter == ScalingFilter::Nis) {
        text += " · sharpness " + std::to_string(settings.sharpnessPercent) + "%";
    }
    return text;
}

} // namespace opensu::settings
