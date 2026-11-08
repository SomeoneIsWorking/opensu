#include "video_settings.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

#include <nlohmann/json.hpp>

namespace opensu::settings {
namespace {

bool touchSizeFits(int size) {
    return size >= touchSizeSmallest && size <= touchSizeLargest;
}

const ChoiceKey& aspectKey() {
    return videoKeys[0];
}
const ChoiceKey& integerKey() {
    return videoKeys[1];
}
const ChoiceKey& samplingKey() {
    return videoKeys[2];
}
const ChoiceKey& filterKey() {
    return videoKeys[3];
}
const ChoiceKey& shaderResolutionKey() {
    return videoKeys[4];
}
const ChoiceKey& touchShowKey() {
    return videoKeys[6];
}

} // namespace

const ChoiceKey& turnedKey() {
    return videoKeys[5];
}

std::string turnedValue(bool turned) {
    return std::string{turnedChoices[turned ? 1 : 0].key};
}

void declareVideo(Schema& schema) {
    const std::string section{videoSection};
    const auto withoutShader = [](const SettingLookup& applies) {
        return applies(shaderKey).empty();
    };
    schema.add(choiceSetting(aspectKey(),
                             "The picture's shape: the core's, 4:3, square pixels or "
                             "the whole screen",
                             section));
    schema.add(choiceSetting(
        integerKey(), "Whole multiples of the game's pixels; Overscale fills and crops", section));
    Setting sampling =
        choiceSetting(samplingKey(), "How pixels are smoothed as they are scaled", section);
    sampling.shownWhen = withoutShader;
    schema.add(std::move(sampling));
    Setting filter = choiceSetting(
        filterKey(), "A pass over the picture: scanlines, an LCD grid or a CRT", section);
    filter.shownWhen = withoutShader;
    schema.add(std::move(filter));
    Setting shader;
    shader.key = std::string{shaderKey};
    shader.label = std::string{shaderLabel};
    shader.help = "A RetroArch slang preset; it replaces Sampling and Filter";
    shader.section = section;
    shader.type = SettingType::Path;
    schema.add(std::move(shader));
    Setting resolution = choiceSetting(
        shaderResolutionKey(),
        "Auto draws the preset at a lower resolution while the GPU cannot keep up", section);
    resolution.shownWhen = [](const SettingLookup& applies) {
        return !applies(shaderKey).empty();
    };
    schema.add(std::move(resolution));
    schema.add(choiceSetting(touchShowKey(),
                             "Dynamic hides them while a controller plays and shows them again at "
                             "a touch",
                             section));
    Setting touchSize;
    touchSize.key = std::string{touchSizeKey};
    touchSize.label = "Touch controls size";
    touchSize.help = "How big the on-screen stick, buttons, Coin and Start are, as far as they fit";
    touchSize.section = section;
    touchSize.type = SettingType::Slider;
    touchSize.range = {.low = touchSizeSmallest, .high = touchSizeLargest, .step = 5};
    touchSize.unit = "%";
    touchSize.fallback = std::to_string(touchSizeDefault);
    schema.add(std::move(touchSize));
    schema.add(choiceSetting(turnedKey(),
                             "Android plays this game in portrait; a desktop turns the picture a "
                             "quarter, for a rotated monitor or cabinet",
                             section, SettingType::Toggle));
}

Resolved resolveShader(const SettingLayers& layers, const LayerKeys& keys) {
    return layers.resolve(keys, shaderKey, std::string{});
}

int resolveTouchSize(const SettingLayers& layers, const LayerKeys& keys) {
    const std::string kept =
        layers.resolve(keys, touchSizeKey, std::to_string(touchSizeDefault)).value;
    int size = touchSizeDefault;
    const auto [end, error] = std::from_chars(kept.data(), kept.data() + kept.size(), size);
    const bool whole = error == std::errc{} && end == kept.data() + kept.size();
    return whole && touchSizeFits(size) ? size : touchSizeDefault;
}

VideoSettings resolveVideo(const SettingLayers& layers, const LayerKeys& keys,
                           const std::filesystem::path& presets) {
    const auto index = [&](const ChoiceKey& video) {
        const Resolved resolved =
            layers.resolve(keys, video.key, std::string{video.choices.front().key});
        return choiceIndexOrDefault(video, resolved.value, keys.game);
    };
    const std::string shader = resolveShader(layers, keys).value;
    return VideoSettings{
        .aspect = static_cast<Aspect>(index(aspectKey())),
        .integer = static_cast<IntegerScale>(index(integerKey())),
        .sampling = static_cast<Sampling>(index(samplingKey())),
        .filter = static_cast<Filter>(index(filterKey())),
        .shaderPreset = shader.empty() ? std::filesystem::path{} : presets / shader,
        .shaderResolution = static_cast<ShaderResolution>(index(shaderResolutionKey())),
        .touchShow = static_cast<TouchShow>(index(touchShowKey())),
        .touchSize = resolveTouchSize(layers, keys),
        .turned = index(turnedKey()) == 1,
    };
}

std::string videoJson(const VideoSettings& video) {
    const auto spelling = [](const ChoiceKey& key, auto value) {
        return std::string{key.choices[static_cast<std::size_t>(value)].key};
    };
    const nlohmann::json document{
        {std::string{aspectKey().key}, spelling(aspectKey(), video.aspect)},
        {std::string{integerKey().key}, spelling(integerKey(), video.integer)},
        {std::string{samplingKey().key}, spelling(samplingKey(), video.sampling)},
        {std::string{filterKey().key}, spelling(filterKey(), video.filter)},
        {std::string{shaderKey}, video.shaderPreset.string()},
        {std::string{shaderResolutionKey().key},
         spelling(shaderResolutionKey(), video.shaderResolution)},
        {std::string{touchShowKey().key}, spelling(touchShowKey(), video.touchShow)},
        {std::string{touchSizeKey}, video.touchSize},
        {std::string{turnedKey().key}, turnedValue(video.turned)},
    };
    return document.dump();
}

std::optional<VideoSettings> parseVideoJson(std::string_view text, std::string& error) {
    const nlohmann::json document = nlohmann::json::parse(text, nullptr, false);
    if (!document.is_object()) {
        error = "not a JSON object";
        return std::nullopt;
    }
    std::array<std::size_t, videoKeys.size()> chosen{};
    for (std::size_t at = 0; at < videoKeys.size(); ++at) {
        const ChoiceKey& video = videoKeys[at];
        const auto found = document.find(std::string{video.key});
        if (found == document.end()) {
            continue;
        }
        const std::optional<std::size_t> index =
            found->is_string() ? choiceIndex(video, found->get<std::string>()) : std::nullopt;
        if (!index) {
            error = found->dump() + " is not a " + std::string{video.key} + " choice";
            return std::nullopt;
        }
        chosen[at] = *index;
    }
    std::filesystem::path shader;
    if (const auto found = document.find(std::string{shaderKey}); found != document.end()) {
        if (!found->is_string()) {
            error = found->dump() + " is not a " + std::string{shaderKey} + " path";
            return std::nullopt;
        }
        shader = found->get<std::string>();
    }
    int touchSize = touchSizeDefault;
    if (const auto found = document.find(std::string{touchSizeKey}); found != document.end()) {
        if (!found->is_number_integer() || !touchSizeFits(found->get<int>())) {
            error = found->dump() + " is not a " + std::string{touchSizeKey};
            return std::nullopt;
        }
        touchSize = found->get<int>();
    }
    return VideoSettings{
        .aspect = static_cast<Aspect>(chosen[0]),
        .integer = static_cast<IntegerScale>(chosen[1]),
        .sampling = static_cast<Sampling>(chosen[2]),
        .filter = static_cast<Filter>(chosen[3]),
        .shaderPreset = std::move(shader),
        .shaderResolution = static_cast<ShaderResolution>(chosen[4]),
        .touchShow = static_cast<TouchShow>(chosen[6]),
        .touchSize = touchSize,
        .turned = chosen[5] == 1,
    };
}

} // namespace opensu::settings
