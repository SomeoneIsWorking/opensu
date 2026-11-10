#include "visual_presets.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

constexpr std::string_view presetsFile = "visual-presets.json";
constexpr std::string_view artExtension = ".png";
constexpr std::size_t sha256HexLength = 64;

/// `object`'s string `key`; empty when it has none and it is optional.
bool textOf(const json& object, std::string_view key, bool required, std::string& out,
            std::string& error) {
    const auto found = object.find(key);
    if (found == object.end()) {
        if (required) {
            error = "no \"" + std::string{key} + "\" in " + object.dump();
        }
        return !required;
    }
    if (!found->is_string() || found->get_ref<const std::string&>().empty()) {
        error = "\"" + std::string{key} + "\" is not a name in " + object.dump();
        return false;
    }
    out = found->get<std::string>();
    return true;
}

bool lowerHex(std::string_view text) {
    return text.size() == sha256HexLength && std::ranges::all_of(text, [](char digit) {
               return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
           });
}

std::optional<PresetArt> artOf(const json& object, std::string& error) {
    PresetArt art;
    if (!object.is_object() || !textOf(object, "id", true, art.id, error) ||
        !textOf(object, "url", true, art.url, error) ||
        !textOf(object, "sha256", true, art.sha256, error)) {
        if (error.empty()) {
            error = object.dump() + " is not art";
        }
        return std::nullopt;
    }
    const auto size = object.find("size");
    if (size == object.end() || !size->is_number_unsigned() || size->get<std::uint64_t>() == 0) {
        error = art.id + " has no size";
        return std::nullopt;
    }
    art.size = size->get<std::uint64_t>();
    if (!art.url.starts_with("https://") || net::originOf(art.url).empty()) {
        error = art.id + "'s URL is not HTTPS";
        return std::nullopt;
    }
    if (!lowerHex(art.sha256)) {
        error = art.id + "'s sha256 is not 64 lower-case hex digits";
        return std::nullopt;
    }
    return art;
}

bool layoutOf(const json& preset, std::string_view key, std::optional<PresetLayout>& layout,
              std::string& error) {
    const auto found = preset.find(key);
    if (found == preset.end()) {
        return true;
    }
    PresetLayout read;
    const auto picture = found->is_object() ? found->find("picture") : found->end();
    const bool shaped = found->is_object() && textOf(*found, "art", true, read.art, error) &&
                        picture != found->end() && picture->is_array() && picture->size() == 4 &&
                        std::ranges::all_of(*picture, [](const json& side) {
                            return side.is_number_unsigned();
                        });
    if (!shaped || (*picture)[2].get<std::uint32_t>() == 0 ||
        (*picture)[3].get<std::uint32_t>() == 0) {
        error = "\"" + std::string{key} + "\" is not {\"art\", \"picture\": [x, y, w, h]}";
        return false;
    }
    read.x = (*picture)[0].get<std::uint32_t>();
    read.y = (*picture)[1].get<std::uint32_t>();
    read.width = (*picture)[2].get<std::uint32_t>();
    read.height = (*picture)[3].get<std::uint32_t>();
    layout = std::move(read);
    return true;
}

/// `spelling` among `key`'s choices into `out`.
template <typename Value>
bool choiceOf(const json& preset, std::string_view name, const settings::ChoiceKey& key,
              std::optional<Value>& out, std::string& error) {
    std::string spelling;
    if (!textOf(preset, name, false, spelling, error)) {
        return false;
    }
    if (spelling.empty()) {
        return true;
    }
    const std::optional<std::size_t> index = settings::choiceIndex(key, spelling);
    if (!index) {
        error = "\"" + spelling + "\" is no " + std::string{key.key};
        return false;
    }
    out = static_cast<Value>(*index);
    return true;
}

std::optional<VisualPreset> presetOf(const json& object, std::string& error) {
    VisualPreset preset;
    if (!object.is_object() || !textOf(object, "id", true, preset.id, error) ||
        !textOf(object, "shader", false, preset.shader, error)) {
        if (error.empty()) {
            error = object.dump() + " is not a preset";
        }
        return std::nullopt;
    }
    std::optional<settings::Filter> filter;
    if (!choiceOf(object, "filter", settings::filterKey(), filter, error) ||
        !choiceOf(object, "aspect", settings::aspectKey(), preset.aspect, error) ||
        !layoutOf(object, "landscape", preset.landscape, error) ||
        !layoutOf(object, "portrait", preset.portrait, error)) {
        error = preset.id + ": " + error;
        return std::nullopt;
    }
    preset.filter = filter.value_or(settings::Filter::None);
    if (const auto consoles = object.find("consoles"); consoles != object.end()) {
        if (!consoles->is_array() || !std::ranges::all_of(*consoles, [](const json& key) {
                return key.is_string();
            })) {
            error = preset.id + ": \"consoles\" is not a list of console keys";
            return std::nullopt;
        }
        preset.consoles = consoles->get<std::vector<std::string>>();
    }
    return preset;
}

} // namespace

std::optional<VisualPresets> VisualPresets::parse(std::string_view text, fs::path folder,
                                                  std::string& error) {
    const json document = json::parse(text, nullptr, false);
    const auto art = document.is_object() ? document.find("art") : document.end();
    const auto presets = document.is_object() ? document.find("presets") : document.end();
    if (art == document.end() || !art->is_array() || presets == document.end() ||
        !presets->is_array()) {
        error = "not {\"art\": [...], \"presets\": [...]}";
        return std::nullopt;
    }
    VisualPresets catalog;
    catalog.folder_ = std::move(folder);
    for (const json& entry : *art) {
        std::optional<PresetArt> read = artOf(entry, error);
        if (!read) {
            return std::nullopt;
        }
        if (catalog.artNamed(read->id) != nullptr) {
            error = "art " + read->id + " is listed twice";
            return std::nullopt;
        }
        catalog.art_.push_back(std::move(*read));
    }
    for (const json& entry : *presets) {
        std::optional<VisualPreset> read = presetOf(entry, error);
        if (!read) {
            return std::nullopt;
        }
        for (const std::optional<PresetLayout>& layout : {read->landscape, read->portrait}) {
            if (layout && catalog.artNamed(layout->art) == nullptr) {
                error = read->id + " frames with " + layout->art + ", which is not listed";
                return std::nullopt;
            }
        }
        if (catalog.find(read->id) != nullptr || read->id == settings::noPreset ||
            read->id == settings::autoConsolePreset) {
            error = "preset " + read->id + " is listed twice or is a choice of its own";
            return std::nullopt;
        }
        catalog.presets_.push_back(std::move(*read));
    }
    return catalog;
}

VisualPresets VisualPresets::shipped(const fs::path& assetsDir, const fs::path& dataDir) {
    const fs::path file = assetsDir / presetsFile;
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::error("presets", "{}: cannot be read; no visual preset is known", file.string());
        return {};
    }
    const std::string body{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string error;
    std::optional<VisualPresets> catalog = parse(body, dataDir / "overlays", error);
    if (!catalog) {
        lucent::error("presets", "{}: {}; no visual preset is known", file.string(), error);
        return {};
    }
    return std::move(*catalog);
}

const VisualPreset* VisualPresets::find(std::string_view id) const {
    const auto found = std::ranges::find(presets_, id, &VisualPreset::id);
    return found == presets_.end() ? nullptr : &*found;
}

const VisualPreset* VisualPresets::forConsole(std::string_view system) const {
    const auto found = std::ranges::find_if(presets_, [system](const VisualPreset& preset) {
        return std::ranges::find(preset.consoles, system) != preset.consoles.end();
    });
    return found == presets_.end() ? nullptr : &*found;
}

const VisualPreset* VisualPresets::chosen(std::string_view choice, std::string_view system) const {
    if (choice == settings::noPreset) {
        return nullptr;
    }
    if (choice == settings::autoConsolePreset) {
        return forConsole(system);
    }
    return find(choice);
}

const PresetArt* VisualPresets::artNamed(std::string_view id) const {
    const auto found = std::ranges::find(art_, id, &PresetArt::id);
    return found == art_.end() ? nullptr : &*found;
}

fs::path VisualPresets::fileOf(const PresetArt& art) const {
    return folder_ / (art.id + std::string{artExtension});
}

bool VisualPresets::downloaded(const PresetArt& art) const {
    std::error_code ec;
    return !folder_.empty() && fs::file_size(fileOf(art), ec) == art.size && !ec;
}

std::vector<PresetArt> VisualPresets::missing() const {
    std::vector<PresetArt> found;
    std::ranges::copy_if(art_, std::back_inserter(found), [this](const PresetArt& art) {
        return !downloaded(art);
    });
    return found;
}

std::size_t VisualPresets::downloadedCount() const {
    return art_.size() - missing().size();
}

std::uint64_t VisualPresets::totalSize() const {
    std::uint64_t total = 0;
    for (const PresetArt& art : art_) {
        total += art.size;
    }
    return total;
}

std::optional<settings::FrameArt> VisualPresets::frameOf(const PresetLayout& layout) const {
    const PresetArt* art = artNamed(layout.art);
    if (art == nullptr || !downloaded(*art)) {
        return std::nullopt;
    }
    return settings::FrameArt{.image = fileOf(*art),
                              .x = layout.x,
                              .y = layout.y,
                              .width = layout.width,
                              .height = layout.height};
}

bool VisualPresets::apply(const VisualPreset& preset, settings::VideoSettings& video,
                          const fs::path& shaderRoot) const {
    std::error_code ec;
    const fs::path shader = preset.shader.empty() ? fs::path{} : shaderRoot / preset.shader;
    const bool shaderThere = !shader.empty() && fs::is_regular_file(shader, ec);
    video.shaderPreset = shaderThere ? shader : fs::path{};
    video.filter = preset.filter;
    if (preset.aspect) {
        video.aspect = *preset.aspect;
    }
    bool whole = true;
    const auto frame = [this, &whole](const std::optional<PresetLayout>& layout) {
        if (!layout) {
            return std::optional<settings::FrameArt>{};
        }
        std::optional<settings::FrameArt> art = frameOf(*layout);
        whole = whole && art.has_value();
        return art;
    };
    video.landscapeArt = frame(preset.landscape);
    video.portraitArt = frame(preset.portrait);
    return whole;
}

net::Result<std::size_t> VisualPresets::install(const std::stop_token& stop,
                                                const EmulatorRelease& release,
                                                const net::WebClient::Progress& progress) const {
    if (folder_.empty()) {
        return net::Failure{"openSU has no folder to keep the overlay art in"};
    }
    std::error_code ec;
    fs::create_directories(folder_, ec);
    if (ec) {
        return net::Failure{"cannot make " + folder_.string() + ": " + ec.message()};
    }
    const std::vector<PresetArt> wanted = missing();
    std::uint64_t total = 0;
    for (const PresetArt& art : wanted) {
        total += art.size;
    }
    std::uint64_t before = 0;
    for (const PresetArt& art : wanted) {
        const fs::path file = fileOf(art);
        const fs::path part = file.string() + ".part";
        const std::string origin = net::originOf(art.url);
        const ReleaseAsset asset{.version = "",
                                 .assetId = art.id,
                                 .name = file.filename().string(),
                                 .url = art.url,
                                 .size = art.size,
                                 .sha256 = art.sha256};
        const net::Result<std::string> fetched = release.fetch(
            stop, asset, part,
            [&progress, before, total](std::uint64_t received, std::uint64_t) {
                return !progress || progress(before + received, total);
            },
            [&origin](const std::string& url) {
                if (url.starts_with(origin)) {
                    return std::string{};
                }
                std::string refused = url;
                refused += " is not on ";
                refused += origin;
                return refused;
            });
        if (!fetched) {
            return fetched.as<std::size_t>(art.id + ": ");
        }
        fs::rename(part, file, ec);
        if (ec) {
            fs::remove(part, ec);
            return net::Failure{"cannot put " + file.string() + " in place: " + ec.message()};
        }
        before += art.size;
        lucent::info("presets", "{} downloaded into {}", art.id, file.string());
    }
    return wanted.size();
}

bool VisualPresets::remove(std::string& error) const {
    for (const PresetArt& art : art_) {
        std::error_code ec;
        fs::remove(fileOf(art), ec);
        if (ec) {
            error = "cannot remove " + fileOf(art).string() + ": " + ec.message();
            return false;
        }
    }
    lucent::info("presets", "overlay art removed from {}", folder_.string());
    return true;
}

} // namespace opensu::library::roms
