#include "roms.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

#include "dump_info.hpp"
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

std::string entryName(const RomNode& entry) {
    return entry.folder ? entry.name : fs::path{entry.name}.stem().string();
}

/// The name the game is listed under: the arcade listing's description for a short name it
/// knows, which also names its box art, else the entry's own name.
std::string listedName(const std::string& name, const NameMap* arcade) {
    if (arcade != nullptr) {
        if (const auto found = arcade->find(lowerAscii(name)); found != arcade->end()) {
            return found->second;
        }
    }
    return name;
}

std::string missingEmulator(const RomSystem& system, const Emulators& emulators) {
    for (const EmulatorEntry& entry : emulators.catalog().entries()) {
        if (const EmulatorInstall* install = emulators.installed(entry.id);
            install != nullptr && entry.serves(system.key) && !install->problem.empty()) {
            return entry.name + " cannot start: " + install->problem;
        }
    }
    const std::vector<std::string_view> names = emulators.candidates(system.key);
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

Provider::Provider(std::vector<fs::path> roots, EmulatorSetup emulators, NameDb names,
                   RomsetLists* romsets, SwitchInfo switchInfo)
    : roots_{std::move(roots)}, emulators_{std::move(emulators)}, names_{std::move(names)},
      romsets_{romsets == nullptr ? RomsetMatcher{} : RomsetMatcher{*romsets}},
      switch_{std::move(switchInfo)} {
}

std::vector<Game> Provider::list() {
    std::vector<Game> games;
    const NameMap arcade = names_.load();
    const Emulators emulators = emulators_.discover();
    std::vector<const EmulatorEntry*> arcadeCores;
    for (const EmulatorEntry& entry : emulators.catalog().entries()) {
        if (entry.serves("arcade") && !entry.romsets.empty()) {
            arcadeCores.push_back(&entry);
        }
    }
    romsets_.begin(RomsetMatcher::Clock::now());
    switch_.begin();
    for (const fs::path& root : roots_) {
        const std::optional<std::vector<RomNode>> folders = tree_->children(root);
        if (!folders) {
            lucent::warn("roms", "{}: not a folder openSU can read", root.string());
            continue;
        }
        for (const RomNode& folder : *folders) {
            const RomSystem* system = systemForFolder(folder.name);
            if (system == nullptr || !folder.folder) {
                continue;
            }
            for (const RomNode& entry :
                 tree_->children(folder.location).value_or(std::vector<RomNode>{})) {
                const std::optional<fs::path> file = gameFile(*tree_, *system, entry);
                if (!file) {
                    continue;
                }
                Game game;
                game.id = "rom:" + entry.location.string();
                game.source = Source::Rom;
                game.sourceId = std::string{system->key};
                const std::string name = entryName(entry);
                const std::string listed =
                    listedName(name, system->key == "arcade" ? &arcade : nullptr);
                // A game's own data is read where it opens as a file; a picked folder's are not.
                std::optional<DumpInfo> dump;
                if (entry.local) {
                    dump = system->key == "switch" ? switch_.read(*file)
                                                   : readDumpInfo(*system, entry.location);
                }
                const bool titled = dump && !dump->title.empty();
                game.title = titled ? dump->title : cleanTitle(listed);
                game.artworkKey = titled ? dump->title : listed;
                if (dump) {
                    game.artwork = dump->icon;
                }
                game.file = *file;
                // The file itself is what makes the entry playable.
                game.installed = true;
                // The emulator's command line carries the game's file.
                game.processHint = entry.folder ? file->filename().string() : entry.name;
                game.emulatorOptions = emulators.options(system->key, *file);
                if (std::optional<LaunchSpec> spec = emulators.launch(system->key, *file)) {
                    game.launch = std::move(*spec);
                    game.emulator = emulators.chosenName(system->key);
                } else {
                    game.unavailable = missingEmulator(*system, emulators);
                }
                if (entry.local && system->key == "arcade" && file->extension() == ".zip") {
                    applyRomsets(game, lowerAscii(file->stem().string()),
                                 romsets_.check(*file, arcadeCores));
                }
                games.push_back(std::move(game));
            }
        }
    }
    romsets_.save();
    switch_.finish();
    return games;
}

} // namespace opensu::library::roms
