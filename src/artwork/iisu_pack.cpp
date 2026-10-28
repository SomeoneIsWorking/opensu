#include "iisu_pack.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <nlohmann/json.hpp>

namespace opensu::artwork {
namespace {

constexpr std::string_view starterPackEntry = "assets/iiSU_StarterPack.zip";
constexpr std::string_view borderDirectory = "assets/borders/";
constexpr std::string_view borderPackEntry = "assets/borders/border_pack.json";
constexpr std::string_view cardDirectory = "platforms/";
constexpr std::string_view cardExtension = ".webp";

std::string fold(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string cardEntry(std::string_view system) {
    return std::string{cardDirectory} + std::string{system} + std::string{cardExtension};
}

bool isCard(const zip::Entry& entry) {
    return entry.name.starts_with(cardDirectory) && entry.name.ends_with(cardExtension);
}

/// A logo the border pack names, which must be a file beside it.
bool plainName(std::string_view name) {
    return !name.empty() && name.find('/') == std::string_view::npos &&
           name.find('\\') == std::string_view::npos && name != "." && name != "..";
}

} // namespace

IisuPack::IisuPack(std::filesystem::path file, ApkArchive archive, bool apk)
    : file_{std::move(file)}, apk_{apk}, archive_{std::move(archive)} {
}

std::shared_ptr<const IisuPack> IisuPack::open(const std::filesystem::path& file,
                                               std::string& error) {
    const std::string name = file.filename().string();
    std::optional<ApkArchive> archive = ApkArchive::overFile(file, error);
    if (!archive) {
        return nullptr;
    }
    std::string reason;
    const std::vector<zip::Entry>* entries = archive->entries(reason);
    if (entries == nullptr) {
        error = name + " is not an APK or zip file (" + reason + ")";
        return nullptr;
    }
    const bool apk = std::ranges::any_of(*entries, [](const zip::Entry& entry) {
        return entry.name == starterPackEntry;
    });
    if (!apk && !std::ranges::any_of(*entries, isCard)) {
        error = name + " is not iiSU's release or starter pack: it holds neither " +
                std::string{starterPackEntry} + " nor platforms/<console>.webp cards";
        return nullptr;
    }
    return std::shared_ptr<const IisuPack>{new IisuPack{file, std::move(*archive), apk}};
}

const zip::Archive* IisuPack::starterPack(std::string& error) const {
    if (starterPack_) {
        return &*starterPack_;
    }
    const zip::Entry* entry = archive_.find(starterPackEntry, error);
    if (entry == nullptr) {
        return nullptr;
    }
    std::optional<std::string> bytes = archive_.extract(*entry, error);
    if (!bytes) {
        return nullptr;
    }
    std::optional<zip::Archive> pack = zip::Archive::open(std::move(*bytes), error);
    if (!pack) {
        error = std::string{starterPackEntry} + " is not a zip: " + error;
        return nullptr;
    }
    starterPack_ = std::move(pack);
    return &*starterPack_;
}

const IisuPack::Logos* IisuPack::logos(std::string& error) const {
    if (logos_) {
        return &*logos_;
    }
    const zip::Entry* entry = archive_.find(borderPackEntry, error);
    if (entry == nullptr) {
        if (error.empty()) {
            // A release without the border pack has cards and no glyphs.
            logos_.emplace();
        }
        return logos_ ? &*logos_ : nullptr;
    }
    const std::optional<std::string> body = archive_.extract(*entry, error);
    if (!body) {
        return nullptr;
    }
    const nlohmann::json document = nlohmann::json::parse(*body, nullptr, false);
    if (!document.is_object() || !document.contains("consoles") ||
        !document["consoles"].is_array()) {
        error = std::string{borderPackEntry} + " has no console list";
        return nullptr;
    }
    Logos logos;
    for (const nlohmann::json& console : document["consoles"]) {
        const auto name = console.find("console");
        const auto logo = console.find("logo");
        if (name != console.end() && logo != console.end() && name->is_string() &&
            logo->is_string() && plainName(logo->get<std::string>())) {
            logos.emplace(fold(name->get<std::string>()), logo->get<std::string>());
        }
    }
    logos_ = std::move(logos);
    return &*logos_;
}

std::optional<std::string> IisuPack::card(std::string_view system, std::string& error) const {
    error.clear();
    const std::lock_guard lock{mutex_};
    if (!apk_) {
        const zip::Entry* entry = archive_.find(cardEntry(system), error);
        return entry == nullptr ? std::nullopt : archive_.extract(*entry, error);
    }
    const zip::Archive* pack = starterPack(error);
    if (pack == nullptr) {
        return std::nullopt;
    }
    return pack->read(cardEntry(system), error);
}

std::optional<std::string> IisuPack::glyph(std::string_view system, std::string& error) const {
    error.clear();
    if (!apk_) {
        return std::nullopt;
    }
    const std::lock_guard lock{mutex_};
    const Logos* map = logos(error);
    if (map == nullptr) {
        return std::nullopt;
    }
    const auto logo = map->find(system);
    if (logo == map->end()) {
        return std::nullopt;
    }
    const zip::Entry* entry = archive_.find(std::string{borderDirectory} + logo->second, error);
    return entry == nullptr ? std::nullopt : archive_.extract(*entry, error);
}

} // namespace opensu::artwork
