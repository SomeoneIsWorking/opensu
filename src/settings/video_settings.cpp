#include "video_settings.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::settings {
namespace {

const VideoKey& aspectKey() {
    return videoKeys[0];
}
const VideoKey& integerKey() {
    return videoKeys[1];
}
const VideoKey& samplingKey() {
    return videoKeys[2];
}
const VideoKey& filterKey() {
    return videoKeys[3];
}

/// `spelling`'s place among `video`'s choices, or the default's (0) with a warning naming `where`.
std::size_t indexOrDefault(const VideoKey& video, std::string_view spelling,
                           std::string_view where) {
    if (const std::optional<std::size_t> index = choiceIndex(video, spelling)) {
        return *index;
    }
    lucent::warn("settings", "{}: {} is not a {} choice; using {}", where, spelling, video.key,
                 video.choices.front().key);
    return 0;
}

} // namespace

bool replacedByShader(std::string_view key) noexcept {
    return key == samplingKey().key || key == filterKey().key;
}

const VideoKey* videoKey(std::string_view key) {
    const auto found = std::ranges::find(videoKeys, key, &VideoKey::key);
    return found == videoKeys.end() ? nullptr : &*found;
}

std::optional<std::size_t> choiceIndex(const VideoKey& video, std::string_view spelling) {
    const auto found = std::ranges::find(video.choices, spelling, &ChoiceText::key);
    if (found == video.choices.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - video.choices.begin());
}

std::string_view choiceLabel(const VideoKey& video, std::string_view spelling) {
    const std::optional<std::size_t> index = choiceIndex(video, spelling);
    return index ? video.choices[*index].label : spelling;
}

Resolved resolveShader(const SettingLayers& layers, const LayerKeys& keys) {
    return layers.resolve(keys, shaderKey, std::string{});
}

VideoSettings resolveVideo(const SettingLayers& layers, const LayerKeys& keys,
                           const std::filesystem::path& presets) {
    const auto index = [&](const VideoKey& video) {
        const Resolved resolved =
            layers.resolve(keys, video.key, std::string{video.choices.front().key});
        return indexOrDefault(video, resolved.value, keys.game);
    };
    const std::string shader = resolveShader(layers, keys).value;
    return VideoSettings{
        .aspect = static_cast<Aspect>(index(aspectKey())),
        .integer = static_cast<IntegerScale>(index(integerKey())),
        .sampling = static_cast<Sampling>(index(samplingKey())),
        .filter = static_cast<Filter>(index(filterKey())),
        .shaderPreset = shader.empty() ? std::filesystem::path{} : presets / shader,
    };
}

std::string launchFileText(const VideoSettings& video) {
    const auto spelling = [](const VideoKey& key, auto value) {
        return std::string{key.choices[static_cast<std::size_t>(value)].key};
    };
    const nlohmann::json document{
        {std::string{aspectKey().key}, spelling(aspectKey(), video.aspect)},
        {std::string{integerKey().key}, spelling(integerKey(), video.integer)},
        {std::string{samplingKey().key}, spelling(samplingKey(), video.sampling)},
        {std::string{filterKey().key}, spelling(filterKey(), video.filter)},
        {std::string{shaderKey}, video.shaderPreset.string()},
    };
    return document.dump(2) + "\n";
}

std::optional<VideoSettings> parseLaunchFile(std::string_view text, std::string& error) {
    const nlohmann::json document = nlohmann::json::parse(text, nullptr, false);
    if (!document.is_object()) {
        error = "not a JSON object";
        return std::nullopt;
    }
    std::array<std::size_t, videoKeys.size()> chosen{};
    for (std::size_t at = 0; at < videoKeys.size(); ++at) {
        const VideoKey& video = videoKeys[at];
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
    return VideoSettings{
        .aspect = static_cast<Aspect>(chosen[0]),
        .integer = static_cast<IntegerScale>(chosen[1]),
        .sampling = static_cast<Sampling>(chosen[2]),
        .filter = static_cast<Filter>(chosen[3]),
        .shaderPreset = std::move(shader),
    };
}

} // namespace opensu::settings
