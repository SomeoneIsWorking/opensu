#include "artwork_store.hpp"

#include <fstream>
#include <system_error>

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

bool writeWhole(const fs::path& file, std::string_view bytes, std::string& error) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    fs::path partial = file;
    partial += ".part";
    {
        std::ofstream out{partial, std::ios::binary | std::ios::trunc};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            error = "cannot write " + partial.string();
            return false;
        }
    }
    fs::rename(partial, file, ec);
    if (ec) {
        error = "cannot replace " + file.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool fresh(const fs::path& file, ArtworkStore::Clock::time_point now, std::chrono::hours lifetime) {
    std::error_code ec;
    const fs::file_time_type written = fs::last_write_time(file, ec);
    return !ec && now - written < lifetime;
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
    case library::Source::Gog:
        break;
    }
    return {};
}

void ArtworkStore::apply(std::vector<library::Game>& games) const {
    for (library::Game& game : games) {
        if (!game.artwork.empty()) {
            continue;
        }
        const fs::path file = pathFor(game);
        std::error_code ec;
        if (!file.empty() && fs::is_regular_file(file, ec)) {
            game.artwork = file;
        }
    }
}

bool ArtworkStore::wanted(const library::Game& game, Clock::time_point now) const {
    if (!game.artwork.empty()) {
        return false;
    }
    const fs::path file = pathFor(game);
    if (file.empty()) {
        return false;
    }
    std::error_code ec;
    return !fs::is_regular_file(file, ec) && !fresh(missMarker(file), now, missLifetime);
}

bool ArtworkStore::save(const library::Game& game, std::string_view bytes,
                        std::string& error) const {
    const fs::path file = pathFor(game);
    if (file.empty()) {
        error = game.title + " has no artwork source";
        return false;
    }
    return writeWhole(file, bytes, error);
}

void ArtworkStore::recordMiss(const library::Game& game) const {
    const fs::path file = pathFor(game);
    std::string error;
    if (!file.empty() && !writeWhole(missMarker(file), {}, error)) {
        lucent::warn("artwork", "{}", error);
    }
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
    if (!writeWhole(root_ / "libretro" / (std::string{system} + ".txt"), text, error)) {
        lucent::warn("artwork", "{}", error);
    }
}

} // namespace iideck::artwork
