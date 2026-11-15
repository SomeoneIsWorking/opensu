#include "video_settings.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

#include <nlohmann/json.hpp>

namespace opensu::settings {
namespace {

constexpr std::string_view landscapeArtKey = "video.art.landscape";
constexpr std::string_view portraitArtKey = "video.art.portrait";

bool withoutVisualPreset(const SettingLookup& applies) {
    const std::string preset = applies(presetKey.key);
    return preset.empty() || preset == noPreset;
}

constexpr std::string_view frameGenKey = "framegen";

/// `settings` when openSU's generator makes them, which the runner does.
std::optional<FrameGenSettings> ownFrameGen(const FrameGenSettings& settings) {
    if (settings.mode == FrameGenMode::Off || settings.generator != FrameGenGenerator::OpenSU) {
        return std::nullopt;
    }
    return settings;
}

nlohmann::json frameGenJson(const std::optional<FrameGenSettings>& settings) {
    if (!settings) {
        return nullptr;
    }
    return nlohmann::json{{"mode", settings->mode == FrameGenMode::Fixed ? "fixed" : "adaptive"},
                          {"ratio", settings->ratioThousandths},
                          {"target", settings->targetThousandths},
                          {"max", settings->maxGenerated},
                          {"tolerance", settings->toleranceThousandths}};
}

bool parseFrameGen(const nlohmann::json& found, std::optional<FrameGenSettings>& settings,
                   std::string& error) {
    if (found.is_null()) {
        return true;
    }
    const auto refused = [&found, &error] {
        error = found.dump() + " is not a " + std::string{frameGenKey};
        return false;
    };
    if (!found.is_object()) {
        return refused();
    }
    const auto mode = found.find("mode");
    if (mode == found.end() || !mode->is_string() || (*mode != "fixed" && *mode != "adaptive")) {
        return refused();
    }
    std::array<std::uint32_t, 4> numbers{};
    for (std::size_t at = 0; const char* key : {"ratio", "target", "max", "tolerance"}) {
        const auto value = found.find(key);
        if (value == found.end() || !value->is_number_unsigned()) {
            return refused();
        }
        numbers[at++] = value->get<std::uint32_t>();
    }
    settings = FrameGenSettings{
        .mode = *mode == "fixed" ? FrameGenMode::Fixed : FrameGenMode::Adaptive,
        .generator = FrameGenGenerator::OpenSU,
        .ratioThousandths = numbers[0],
        .targetThousandths = numbers[1],
        .maxGenerated = numbers[2],
        .toleranceThousandths = numbers[3],
    };
    return true;
}

nlohmann::json artJson(const std::optional<FrameArt>& art) {
    if (!art) {
        return nullptr;
    }
    return nlohmann::json{{"image", art->image.string()},
                          {"picture", {art->x, art->y, art->width, art->height}}};
}

/// The art in `found` into `art`, which stays none for null; false with `error` when it is not
/// art.
bool parseArt(const nlohmann::json& found, std::string_view key, std::optional<FrameArt>& art,
              std::string& error) {
    if (found.is_null()) {
        return true;
    }
    const auto image = found.find("image");
    const auto picture = found.find("picture");
    const bool shaped = found.is_object() && image != found.end() && image->is_string() &&
                        picture != found.end() && picture->is_array() && picture->size() == 4 &&
                        std::ranges::all_of(*picture, [](const nlohmann::json& side) {
                            return side.is_number_unsigned();
                        });
    if (!shaped || (*picture)[2].get<std::uint32_t>() == 0 ||
        (*picture)[3].get<std::uint32_t>() == 0) {
        error = found.dump() + " is not a " + std::string{key};
        return false;
    }
    art = FrameArt{.image = image->get<std::string>(),
                   .x = (*picture)[0].get<std::uint32_t>(),
                   .y = (*picture)[1].get<std::uint32_t>(),
                   .width = (*picture)[2].get<std::uint32_t>(),
                   .height = (*picture)[3].get<std::uint32_t>()};
    return true;
}

bool touchSizeFits(int size) {
    return size >= touchSizeSmallest && size <= touchSizeLargest;
}

const ChoiceKey& integerKey() {
    return videoKeys[1];
}
const ChoiceKey& samplingKey() {
    return videoKeys[2];
}
const ChoiceKey& shaderResolutionKey() {
    return videoKeys[4];
}
const ChoiceKey& touchShowKey() {
    return videoKeys[6];
}

} // namespace

const ChoiceKey& aspectKey() {
    return videoKeys[0];
}

const ChoiceKey& filterKey() {
    return videoKeys[3];
}

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
    schema.add(choiceSetting(presetKey,
                             "A look set at once: CRT's shader and television, or the frame of "
                             "the game's own console with Auto Console; " +
                                 std::string{presetArtCredit},
                             section, SettingType::Preset));
    Setting aspect = choiceSetting(aspectKey(),
                                   "The picture's shape: the core's, 4:3, square pixels or "
                                   "the whole screen",
                                   section);
    aspect.shownWhen = withoutVisualPreset;
    schema.add(std::move(aspect));
    schema.add(choiceSetting(
        integerKey(), "Whole multiples of the game's pixels; Overscale fills and crops", section));
    Setting sampling =
        choiceSetting(samplingKey(), "How pixels are smoothed as they are scaled", section);
    sampling.shownWhen = withoutShader;
    schema.add(std::move(sampling));
    Setting filter = choiceSetting(
        filterKey(), "A pass over the picture: scanlines, an LCD grid or a CRT", section);
    filter.shownWhen = [withoutShader](const SettingLookup& applies) {
        return withoutShader(applies) && withoutVisualPreset(applies);
    };
    schema.add(std::move(filter));
    Setting shader;
    shader.key = std::string{shaderKey};
    shader.label = std::string{shaderLabel};
    shader.help = "A RetroArch slang preset; it replaces Sampling and Filter";
    shader.section = section;
    shader.type = SettingType::Path;
    shader.shownWhen = withoutVisualPreset;
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

std::string resolvePreset(const SettingLayers& layers, const LayerKeys& keys) {
    const Resolved resolved = layers.resolve(keys, presetKey.key, std::string{noPreset});
    return std::string{
        presetChoices[choiceIndexOrDefault(presetKey, resolved.value, keys.game)].key};
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
        .frameGen = ownFrameGen(resolveFrameGen(layers, keys)),
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
        {std::string{landscapeArtKey}, artJson(video.landscapeArt)},
        {std::string{portraitArtKey}, artJson(video.portraitArt)},
        {std::string{frameGenKey}, frameGenJson(video.frameGen)},
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
    std::array<std::optional<FrameArt>, 2> art;
    for (std::size_t at = 0; const std::string_view key : {landscapeArtKey, portraitArtKey}) {
        if (const auto found = document.find(std::string{key}); found != document.end()) {
            if (!parseArt(*found, key, art[at], error)) {
                return std::nullopt;
            }
        }
        ++at;
    }
    std::optional<FrameGenSettings> frameGen;
    if (const auto found = document.find(std::string{frameGenKey}); found != document.end()) {
        if (!parseFrameGen(*found, frameGen, error)) {
            return std::nullopt;
        }
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
        .landscapeArt = std::move(art[0]),
        .portraitArt = std::move(art[1]),
        .frameGen = frameGen,
    };
}

} // namespace opensu::settings
