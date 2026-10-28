#include "console_theme.hpp"

#include <array>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

namespace opensu::artwork {
namespace {

constexpr const char* manifestFile = "theme.json";
constexpr std::array maskExtensions{".svg"};
constexpr std::array pictureExtensions{".webp", ".png", ".jpg"};

/// Each system's file in `folder`, by file stem; a system with several takes the first in name
/// order.
std::map<std::string, std::filesystem::path, std::less<>>
filesIn(const std::filesystem::path& folder) {
    std::map<std::string, std::filesystem::path, std::less<>> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{folder, ec}) {
        const std::string extension = entry.path().extension().string();
        const bool known =
            std::ranges::find(maskExtensions, extension) != maskExtensions.end() ||
            std::ranges::find(pictureExtensions, extension) != pictureExtensions.end();
        if (!known || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string system = entry.path().stem().string();
        const auto found = files.find(system);
        if (found == files.end() || entry.path() < found->second) {
            files[system] = entry.path();
        }
    }
    return files;
}

std::optional<ThemeArt>
artOf(const std::map<std::string, std::filesystem::path, std::less<>>& files,
      std::string_view system) {
    const auto found = files.find(system);
    if (found == files.end()) {
        return std::nullopt;
    }
    const bool mask = found->second.extension() == ".svg";
    return ThemeArt{.kind = mask ? ThemeArt::Kind::Mask : ThemeArt::Kind::Picture,
                    .file = found->second,
                    .key = found->second.string(),
                    .bytes = {}};
}

/// The manifest, or nothing with `error` saying why.
std::optional<nlohmann::json> manifestOf(const std::filesystem::path& folder, std::string& error) {
    const std::filesystem::path file = folder / manifestFile;
    std::ifstream in{file};
    if (!in) {
        error = file.string() + " is missing";
        return std::nullopt;
    }
    nlohmann::json manifest = nlohmann::json::parse(in, nullptr, false);
    if (!manifest.is_object()) {
        error = file.string() + " is not a JSON object";
        return std::nullopt;
    }
    return manifest;
}

std::string text(const nlohmann::json& manifest, const char* key) {
    const auto found = manifest.find(key);
    return found != manifest.end() && found->is_string() ? found->get<std::string>()
                                                         : std::string{};
}

/// One theme picture read out of iiSU's file.
ThemeArt iisuArt(const std::shared_ptr<const IisuPack>& pack, std::string_view system,
                 std::optional<std::string> (IisuPack::*read)(std::string_view, std::string&) const,
                 const char* what) {
    return ThemeArt{.kind = ThemeArt::Kind::Picture,
                    .file = {},
                    .key = pack->file().string() + " " + what + " " + std::string{system},
                    .bytes = [pack, system = std::string{system},
                              read](std::string& error) -> std::optional<std::string> {
                        return ((*pack).*read)(system, error);
                    }};
}

} // namespace

std::shared_ptr<const FolderTheme> FolderTheme::open(std::string id,
                                                     const std::filesystem::path& folder,
                                                     const std::filesystem::path& glyphs,
                                                     std::string& error) {
    const std::optional<nlohmann::json> manifest = manifestOf(folder, error);
    if (!manifest) {
        return nullptr;
    }
    std::string name = text(*manifest, "name");
    if (name.empty()) {
        name = folder.filename().string();
    }
    const CardStyle style = text(*manifest, "cards") == "tile" ? CardStyle::Tile : CardStyle::Mark;
    ThemeInfo info{.id = std::move(id), .name = std::move(name), .note = text(*manifest, "note")};
    return std::shared_ptr<const FolderTheme>{
        new FolderTheme{std::move(info), style, filesIn(folder / "cards"), filesIn(glyphs)}};
}

std::string FolderTheme::glyphsFrom(const std::filesystem::path& folder) {
    std::string error;
    const std::optional<nlohmann::json> manifest = manifestOf(folder, error);
    return manifest ? text(*manifest, "glyphs") : std::string{};
}

std::optional<ThemeArt> FolderTheme::card(std::string_view system) const {
    return artOf(cards_, system);
}

std::optional<ThemeArt> FolderTheme::glyph(std::string_view system) const {
    return artOf(glyphs_, system);
}

IisuTheme::IisuTheme(std::shared_ptr<const IisuPack> pack)
    : ConsoleTheme{ThemeInfo{.id = "iisu",
                             .name = "iiSU",
                             .note = "iiSU's, from " + pack->file().filename().string()},
                   CardStyle::Tile},
      pack_{std::move(pack)} {
}

// Whether iiSU's file holds a system's art is known only once it is read, off the frame thread:
// a system it lacks fails there and shows no picture.
std::optional<ThemeArt> IisuTheme::card(std::string_view system) const {
    return iisuArt(pack_, system, &IisuPack::card, "card");
}

std::optional<ThemeArt> IisuTheme::glyph(std::string_view system) const {
    if (!pack_->hasGlyphs()) {
        return std::nullopt;
    }
    return iisuArt(pack_, system, &IisuPack::glyph, "glyph");
}

} // namespace opensu::artwork
