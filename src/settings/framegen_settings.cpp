#include "framegen_settings.hpp"

#include <algorithm>
#include <charconv>
#include <string>

namespace opensu::settings {
namespace {

enum class Key : std::uint8_t { Mode, Ratio, Target, Max, Tolerance, Performance, FlowScale };

const ChoiceKey& keyOf(Key key) {
    return frameGenKeys[static_cast<std::size_t>(key)];
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

} // namespace

const ChoiceKey* frameGenKey(std::string_view key) {
    const auto found = std::ranges::find(frameGenKeys, key, &ChoiceKey::key);
    return found == frameGenKeys.end() ? nullptr : &*found;
}

bool frameGenKeyUsedIn(std::string_view key, FrameGenMode mode) noexcept {
    if (key == keyOf(Key::Mode).key) {
        return true;
    }
    switch (mode) {
    case FrameGenMode::Off:
        return false;
    case FrameGenMode::Fixed:
        return key != keyOf(Key::Target).key && key != keyOf(Key::Max).key &&
               key != keyOf(Key::Tolerance).key;
    case FrameGenMode::Adaptive:
        return key != keyOf(Key::Ratio).key;
    }
    return false;
}

FrameGenMode frameGenModeOf(std::string_view spelling) noexcept {
    const std::optional<std::size_t> index = choiceIndex(keyOf(Key::Mode), spelling);
    return index ? static_cast<FrameGenMode>(*index) : FrameGenMode::Off;
}

FrameGenSettings resolveFrameGen(const SettingLayers& layers, const LayerKeys& keys) {
    const auto spelling = [&](Key key) {
        const ChoiceKey& setting = keyOf(key);
        const Resolved resolved =
            layers.resolve(keys, setting.key, std::string{setting.defaultKey()});
        return setting.choices[choiceIndexOrDefault(setting, resolved.value, keys.game)].key;
    };
    const std::string_view target = spelling(Key::Target);
    return FrameGenSettings{
        .mode = frameGenModeOf(spelling(Key::Mode)),
        .ratioThousandths = thousandths(spelling(Key::Ratio)),
        .targetThousandths = target == keyOf(Key::Target).defaultKey() ? 0 : thousandths(target),
        .maxGenerated = thousandths(spelling(Key::Max)) / 1000,
        .toleranceThousandths = thousandths(spelling(Key::Tolerance)),
        .performance = spelling(Key::Performance) == frameGenPerformanceChoices[1].key,
        .flowScalePercent = thousandths(spelling(Key::FlowScale)) / 1000,
    };
}

} // namespace opensu::settings
