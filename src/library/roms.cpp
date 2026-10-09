#include "roms.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

#include "lucent/log.h"
#include "rom_systems.hpp"
#include "rom_titles.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

std::string lowerKey(std::string_view name) {
    std::string out;
    for (const unsigned char c : name) {
        if (std::isalnum(c) != 0) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::string lowerAscii(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool isRootName(std::string_view name) {
    const std::string key = lowerKey(name);
    return key == "rom" || key == "roms";
}

bool holdsSystems(const fs::path& root) {
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{root, ec}) {
        if (entry.is_directory(ec) &&
            systemForFolder(entry.path().filename().string()) != nullptr) {
            return true;
        }
    }
    return false;
}

/// The children of `dir` named like a ROM root.
void addRootsIn(const fs::path& dir, std::vector<fs::path>& roots) {
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, ec}) {
        if (entry.is_directory(ec) && isRootName(entry.path().filename().string()) &&
            holdsSystems(entry.path())) {
            roots.push_back(entry.path());
        }
    }
}

std::string entryName(const fs::path& entry, bool isFile) {
    return isFile ? entry.stem().string() : entry.filename().string();
}

/// The title of the entry: the arcade listing's description for a short name it knows, else the
/// name itself, cleaned of its dump tags.
std::string titleOf(const std::string& name, const NameMap* arcade) {
    if (arcade != nullptr) {
        if (const auto found = arcade->find(lowerAscii(name)); found != arcade->end()) {
            return cleanTitle(found->second);
        }
    }
    return cleanTitle(name);
}

std::string missingEmulator(const RomSystem& system) {
    const std::vector<std::string_view> names = Emulators::candidates(system.key);
    std::string text = "no " + std::string{system.label} + " emulator found";
    if (!names.empty()) {
        text += "; install ";
        for (std::size_t i = 0; i < names.size(); ++i) {
            text += (i == 0 ? "" : " or ") + std::string{names[i]};
        }
    }
    return text;
}

} // namespace

std::vector<fs::path> standardMountDirs(const fs::path& home) {
    const std::string user = home.filename().string();
    return {"/mnt", "/media/" + user, "/run/media/" + user};
}

std::vector<fs::path> discoverRoots(const fs::path& home, const std::vector<fs::path>& mountDirs) {
    std::vector<fs::path> roots;
    addRootsIn(home, roots);
    addRootsIn(home / "Emulation", roots);
    for (const fs::path& mounts : mountDirs) {
        std::error_code ec;
        for (const fs::directory_entry& drive : fs::directory_iterator{mounts, ec}) {
            if (drive.is_directory(ec)) {
                addRootsIn(drive.path(), roots);
            }
        }
    }
    return roots;
}

Provider::Provider(std::vector<fs::path> roots, Emulators emulators, NameDb names)
    : roots_{std::move(roots)}, emulators_{std::move(emulators)}, names_{std::move(names)} {
}

std::vector<Game> Provider::list() {
    std::vector<Game> games;
    const NameMap arcade = names_.load();
    for (const fs::path& root : roots_) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            lucent::warn("roms", "{}: not a directory", root.string());
            continue;
        }
        for (const fs::directory_entry& folder : fs::directory_iterator{root, ec}) {
            const RomSystem* system = systemForFolder(folder.path().filename().string());
            if (system == nullptr || !folder.is_directory(ec)) {
                continue;
            }
            std::vector<fs::path> entries;
            for (const fs::directory_entry& entry : fs::directory_iterator{folder.path(), ec}) {
                entries.push_back(entry.path());
            }
            std::ranges::sort(entries);
            for (const fs::path& entry : entries) {
                const std::optional<fs::path> file = gameFile(*system, entry);
                if (!file) {
                    continue;
                }
                Game game;
                game.id = "rom:" + entry.string();
                game.source = Source::Rom;
                game.sourceId = std::string{system->key};
                const std::string name = entryName(entry, fs::is_regular_file(entry, ec));
                game.title = titleOf(name, system->key == "arcade" ? &arcade : nullptr);
                game.artworkKey = name;
                // The file itself is what makes the entry playable.
                game.installed = true;
                // The emulator's command line carries the game's file.
                game.processHint = file->filename().string();
                game.emulatorOptions = emulators_.options(system->key, *file);
                if (std::optional<LaunchSpec> spec = emulators_.launch(system->key, *file)) {
                    game.launch = std::move(*spec);
                    game.emulator = emulators_.chosenName(system->key);
                } else {
                    game.unavailable = missingEmulator(*system);
                }
                games.push_back(std::move(game));
            }
        }
    }
    return games;
}

} // namespace opensu::library::roms
