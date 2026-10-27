#include "framegen_settings.hpp"

#include <algorithm>
#include <charconv>
#include <string>

#include "lucent/log.h"

namespace opensu::settings {
namespace {

enum class Key : std::uint8_t { Preset, Mode, Ratio, Target, Max, Tolerance, Performance };
enum class Preset : std::uint8_t { Off, Smooth60, MatchDisplay, Custom };

const ChoiceKey& keyOf(Key key) {
    return frameGenKeys[static_cast<std::size_t>(key)];
}

std::string_view spellingOf(Preset preset) {
    return frameGenPresetChoices[static_cast<std::size_t>(preset)].key;
}

std::string_view fixedMode() {
    return frameGenModeChoices[0].key;
}

std::string_view adaptiveMode() {
    return frameGenModeChoices[1].key;
}

/// A decimal choice spelling ("1.25", "60") in thousandths; the choices are all such numbers.
std::uint32_t thousandths(std::string_view spelling) {
    const std::size_t point = spelling.find('.');
    const std::string_view whole = spelling.substr(0, point);
    std::uint32_t value = 0;
    std::from_chars(whole.data(), whole.data() + whole.size(), value);
    value *= 1000;
    if (point != std::string_view::npos) {
        std::uint32_t scale = 100;
        for (const char digit : spelling.substr(point + 1, 3)) {
            value += static_cast<std::uint32_t>(digit - '0') * scale;
            scale /= 10;
        }
    }
    return value;
}

/// What Smooth 60 and Match display keep at the detail keys.
struct PresetValues {
    std::string_view target;
    std::string_view max;
};
constexpr std::string_view presetTolerance = "5";

PresetValues valuesOf(Preset preset) {
    return preset == Preset::Smooth60 ? PresetValues{"60", "2"} : PresetValues{"display", "3"};
}

/// The setting `key` declares, from its choice table.
Setting choiceSetting(Key key, std::string help, SettingType type = SettingType::Choice) {
    return settings::choiceSetting(keyOf(key), std::move(help), std::string{frameGenSection}, type);
}

/// Shown when the values that apply are Custom's, and in `mode` when one is named.
std::function<bool(const SettingLookup&)> customIn(std::string_view mode) {
    return [mode](const SettingLookup& applies) {
        return applies(keyOf(Key::Preset).key) == spellingOf(Preset::Custom) &&
               (mode.empty() || applies(keyOf(Key::Mode).key) == mode);
    };
}

std::string fps(std::uint32_t thousandthsOfFps) {
    return std::to_string(thousandthsOfFps / 1000) + " fps";
}

} // namespace

void declareFrameGen(Schema& schema) {
    schema.add(choiceSetting(
        Key::Preset, "Smooth 60 fills a game up to 60 fps; Match display up to the screen's rate",
        SettingType::Preset));
    Setting mode = choiceSetting(
        Key::Mode, "Fixed shows a set number of frames per real one; Adaptive only what it takes");
    mode.shownWhen = customIn({});
    schema.add(std::move(mode));
    Setting ratio = choiceSetting(Key::Ratio, "Frames shown for each real frame");
    ratio.shownWhen = customIn(fixedMode());
    schema.add(std::move(ratio));
    Setting target = choiceSetting(Key::Target, "The frame rate Adaptive fills up to");
    target.shownWhen = customIn(adaptiveMode());
    schema.add(std::move(target));
    Setting most = choiceSetting(Key::Max, "The most frames shown for each real frame");
    most.shownWhen = customIn(adaptiveMode());
    schema.add(std::move(most));
    Setting tolerance =
        choiceSetting(Key::Tolerance, "A game this close to the target gets no generated frames");
    tolerance.shownWhen = customIn(adaptiveMode());
    schema.add(std::move(tolerance));
    Setting performance = choiceSetting(
        Key::Performance, "A lighter generator: faster on a weak GPU, a little softer",
        SettingType::Toggle);
    performance.shownWhen = customIn({});
    schema.add(std::move(performance));
    Setting flow;
    flow.key = std::string{frameGenFlowScaleKey};
    flow.label = "Flow scale";
    flow.help = "The size motion is estimated at; lower is faster at high resolutions";
    flow.section = std::string{frameGenSection};
    flow.type = SettingType::Slider;
    flow.range = frameGenFlowScaleRange;
    flow.unit = "%";
    flow.fallback = std::to_string(frameGenFlowScaleDefault);
    flow.shownWhen = customIn({});
    schema.add(std::move(flow));
}

FrameGenSettings resolveFrameGen(const SettingLayers& layers, const LayerKeys& keys) {
    return resolveFrameGen(
        [&layers, &keys](std::string_view key) {
            const auto declared = std::ranges::find(frameGenKeys, key, &ChoiceKey::key);
            const std::string fallback = declared != frameGenKeys.end()
                                             ? std::string{declared->defaultKey()}
                                             : std::to_string(frameGenFlowScaleDefault);
            return layers.resolve(keys, key, fallback).value;
        },
        keys.game);
}

FrameGenSettings resolveFrameGen(const SettingLookup& applies, std::string_view where) {
    const auto spelling = [&](Key key) {
        const ChoiceKey& setting = keyOf(key);
        return setting.choices[choiceIndexOrDefault(setting, applies(setting.key), where)].key;
    };
    const std::string_view preset = spelling(Key::Preset);
    if (preset == spellingOf(Preset::Off)) {
        return FrameGenSettings{};
    }
    if (preset != spellingOf(Preset::Custom)) {
        const PresetValues named = valuesOf(
            preset == spellingOf(Preset::Smooth60) ? Preset::Smooth60 : Preset::MatchDisplay);
        return FrameGenSettings{
            .mode = FrameGenMode::Adaptive,
            .targetThousandths = named.target == "display" ? 0 : thousandths(named.target),
            .maxGenerated = thousandths(named.max) / 1000,
            .toleranceThousandths = thousandths(presetTolerance),
        };
    }
    const std::string_view target = spelling(Key::Target);
    std::uint32_t flow = frameGenFlowScaleDefault;
    const std::string kept = applies(frameGenFlowScaleKey);
    const auto [end, error] = std::from_chars(kept.data(), kept.data() + kept.size(), flow);
    if (error != std::errc{} || end != kept.data() + kept.size() ||
        flow < static_cast<std::uint32_t>(frameGenFlowScaleRange.low) ||
        flow > static_cast<std::uint32_t>(frameGenFlowScaleRange.high)) {
        lucent::warn("settings", "{}: {} is not a flow scale; using {}", where, kept,
                     frameGenFlowScaleDefault);
        flow = frameGenFlowScaleDefault;
    }
    return FrameGenSettings{
        .mode = spelling(Key::Mode) == fixedMode() ? FrameGenMode::Fixed : FrameGenMode::Adaptive,
        .ratioThousandths = thousandths(spelling(Key::Ratio)),
        .targetThousandths = target == keyOf(Key::Target).defaultKey() ? 0 : thousandths(target),
        .maxGenerated = thousandths(spelling(Key::Max)) / 1000,
        .toleranceThousandths = thousandths(spelling(Key::Tolerance)),
        .performance = spelling(Key::Performance) == frameGenPerformanceChoices[1].key,
        .flowScalePercent = flow,
    };
}

std::string frameGenSummary(const FrameGenSettings& settings) {
    std::string text;
    switch (settings.mode) {
    case FrameGenMode::Off:
        return "Off";
    case FrameGenMode::Fixed: {
        const std::uint32_t whole = settings.ratioThousandths / 1000;
        const std::uint32_t part = settings.ratioThousandths % 1000;
        text = "Fixed ratio · " + std::to_string(whole);
        if (part != 0) {
            std::string decimals = std::to_string(part + 1000).substr(1);
            while (decimals.ends_with('0')) {
                decimals.pop_back();
            }
            text += "." + decimals;
        }
        text += "x";
        break;
    }
    case FrameGenMode::Adaptive:
        text = "Adaptive · ";
        text += settings.targetThousandths == 0 ? std::string{"display refresh"}
                                                : "target " + fps(settings.targetThousandths);
        text += " · up to " + std::to_string(settings.maxGenerated + 1) + "x";
        if (settings.toleranceThousandths != 0) {
            text += " · tolerance " + fps(settings.toleranceThousandths);
        }
        break;
    }
    if (settings.performance) {
        text += " · performance";
    }
    if (settings.flowScalePercent != static_cast<std::uint32_t>(frameGenFlowScaleDefault)) {
        text += " · flow " + std::to_string(settings.flowScalePercent) + "%";
    }
    return text;
}

std::vector<std::pair<std::string_view, std::string_view>>
frameGenPresetValues(std::string_view preset) {
    if (preset != spellingOf(Preset::Smooth60) && preset != spellingOf(Preset::MatchDisplay)) {
        return {};
    }
    const PresetValues named =
        valuesOf(preset == spellingOf(Preset::Smooth60) ? Preset::Smooth60 : Preset::MatchDisplay);
    return {{keyOf(Key::Mode).key, adaptiveMode()},
            {keyOf(Key::Target).key, named.target},
            {keyOf(Key::Max).key, named.max},
            {keyOf(Key::Tolerance).key, presetTolerance}};
}

bool moveFrameGenModesToPresets(SettingLayers& layers) {
    const std::string_view mode = keyOf(Key::Mode).key;
    const std::string_view preset = keyOf(Key::Preset).key;
    std::vector<std::pair<Layer, std::string>> older;
    for (const auto& [layer, values] : layers.layers()) {
        const auto kept = values.find(mode);
        if (kept != values.end() && !values.contains(preset)) {
            older.emplace_back(layer, kept->second);
        }
    }
    for (const auto& [layer, value] : older) {
        if (value == "off") {
            layers.clear(layer, mode);
            layers.set(layer, preset, std::string{spellingOf(Preset::Off)});
        } else {
            layers.set(layer, preset, std::string{spellingOf(Preset::Custom)});
        }
    }
    return !older.empty();
}

} // namespace opensu::settings
