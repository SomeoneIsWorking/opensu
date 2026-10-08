#include "artwork_store.hpp"

#include <fstream>
#include <system_error>

#include "fileio/atomic_write.hpp"
#include "libretro_index.hpp"
#include "lucent/log.h"
#include "rom_systems.hpp"

namespace iideck::artwork {
namespace {

namespace fs = std::filesystem;

fs::path missMarker(const fs::path& file) {
    fs::path marker = file;
    marker += ".miss";
    return marker;
}

bool fresh(const fs::path& file, ArtworkStore::Clock::time_point now, std::chrono::hours lifetime) {
    std::error_code ec;
    const fs::file_time_type written = fs::last_write_time(file, ec);
    return !ec && now - written < lifetime;
}

bool isFile(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec);
}

bool wantedAt(const fs::path& file, ArtworkStore::Clock::time_point now) {
    return !file.empty() && !isFile(file) &&
           !fresh(missMarker(file), now, ArtworkStore::missLifetime);
}

bool saveAt(const fs::path& file, const std::string& owner, std::string_view bytes,
            std::string& error) {
    if (file.empty()) {
        error = owner + " has no artwork source";
        return false;
    }
    return fileio::writeWhole(file, bytes, error);
}

void recordMissAt(const fs::path& file) {
    std::string error;
    if (!file.empty() && !fileio::writeWhole(missMarker(file), {}, error)) {
        lucent::warn("artwork", "{}", error);
    }
}

} // namespace

ArtworkStore::ArtworkStore(const fs::path& root) : root_{root} {
}

fs::path ArtworkStore::pathFor(const library::Game& game) const {
    switch (game.source) {
    case library::Source::Steam:
        return game.sourceId.empty() ? fs::path{} : root_ / "steam" / (game.sourceId + ".jpg");
    case library::Source::Rom: {
        const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId);
        if (system == nullptr || system->libretroName.empty() || game.artworkKey.empty()) {
            return {};
        }
        return root_ / "rom" / game.sourceId / (thumbnailName(game.artworkKey) + ".png");
    }
    case library::Source::Epic:
        return game.sourceId.empty() || game.artworkUrl.empty()
                   ? fs::path{}
                   : root_ / "epic" / (game.sourceId + ".jpg");
    case library::Source::Gog:
        break;
    }
    return {};
}

fs::path ArtworkStore::pathFor(const library::Console& console) const {
    return console.system.empty() ? fs::path{} : root_ / "console" / (console.system + ".png");
}

fs::path ArtworkStore::glyphPath(std::string_view system) const {
    return system.empty() ? fs::path{} : root_ / "glyph" / (std::string{system} + ".png");
}

fs::path ArtworkStore::storedGlyph(std::string_view system) const {
    const fs::path file = glyphPath(system);
    return !file.empty() && isFile(file) ? file : fs::path{};
}

fs::path ArtworkStore::assetPath(const ApkAsset& asset) const {
    return root_ / folder(asset.kind) / asset.file;
}

fs::path ArtworkStore::storedAsset(const ApkAsset& asset) const {
    const fs::path file = assetPath(asset);
    return isFile(file) ? file : fs::path{};
}

fs::path ArtworkStore::packPath(std::string_view name) const {
    return root_ / "iisu" / name;
}

void ArtworkStore::apply(std::vector<library::ShelfItem>& shelf) const {
    for (library::ShelfItem& item : shelf) {
        auto* console = std::get_if<library::Console>(&item);
        if (console == nullptr || !console->artwork.empty()) {
            continue;
        }
        const fs::path file = pathFor(*console);
        if (!file.empty() && isFile(file)) {
            console->artwork = file;
        }
    }
}

void ArtworkStore::apply(std::vector<library::Game>& games) const {
    for (library::Game& game : games) {
        if (!game.artwork.empty()) {
            continue;
        }
        const fs::path file = pathFor(game);
        if (!file.empty() && isFile(file)) {
            game.artwork = file;
        }
    }
}

bool ArtworkStore::wanted(const library::Game& game, Clock::time_point now) const {
    return game.artwork.empty() && wantedAt(pathFor(game), now);
}

bool ArtworkStore::wanted(const library::Console& console, Clock::time_point now) const {
    return console.artwork.empty() && wantedAt(pathFor(console), now);
}

bool ArtworkStore::wantedGlyph(std::string_view system, Clock::time_point now) const {
    return wantedAt(glyphPath(system), now);
}

bool ArtworkStore::wantedAsset(const ApkAsset& asset, Clock::time_point now) const {
    return wantedAt(assetPath(asset), now);
}

bool ArtworkStore::saveAsset(const ApkAsset& asset, std::string_view bytes,
                             std::string& error) const {
    return saveAt(assetPath(asset), asset.file, bytes, error);
}

void ArtworkStore::recordAssetMiss(const ApkAsset& asset) const {
    recordMissAt(assetPath(asset));
}

bool ArtworkStore::save(const library::Game& game, std::string_view bytes,
                        std::string& error) const {
    return saveAt(pathFor(game), game.title, bytes, error);
}

bool ArtworkStore::save(const library::Console& console, std::string_view bytes,
                        std::string& error) const {
    return saveAt(pathFor(console), console.label, bytes, error);
}

bool ArtworkStore::saveGlyph(std::string_view system, std::string_view bytes,
                             std::string& error) const {
    return saveAt(glyphPath(system), std::string{system}, bytes, error);
}

bool ArtworkStore::savePack(std::string_view name, std::string_view bytes,
                            std::string& error) const {
    return saveAt(packPath(name), std::string{name}, bytes, error);
}

void ArtworkStore::recordMiss(const library::Game& game) const {
    recordMissAt(pathFor(game));
}

void ArtworkStore::recordMiss(const library::Console& console) const {
    recordMissAt(pathFor(console));
}

void ArtworkStore::recordGlyphMiss(std::string_view system) const {
    recordMissAt(glyphPath(system));
}

std::optional<std::vector<std::string>> ArtworkStore::index(std::string_view system,
                                                            Clock::time_point now) const {
    const fs::path file = root_ / "libretro" / (std::string{system} + ".txt");
    if (!fresh(file, now, indexLifetime)) {
        return std::nullopt;
    }
    std::ifstream in{file};
    std::vector<std::string> names;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty()) {
            names.push_back(line);
        }
    }
    return names;
}

void ArtworkStore::saveIndex(std::string_view system, const std::vector<std::string>& names) const {
    std::string text;
    for (const std::string& name : names) {
        text += name;
        text += '\n';
    }
    std::string error;
    if (!fileio::writeWhole(root_ / "libretro" / (std::string{system} + ".txt"), text, error)) {
        lucent::warn("artwork", "{}", error);
    }
}

} // namespace iideck::artwork
