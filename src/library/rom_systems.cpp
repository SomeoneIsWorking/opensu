#include "rom_systems.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <string>
#include <system_error>

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// How deep a game folder is searched for its file: Switch dumps nest one level, Wii U and PS3
/// games two.
constexpr int folderDepth = 3;

/// A Switch title id in a dump's name: `[0100ABCD12345000]`.
constexpr std::size_t titleIdDigits = 16;

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string folderKey(std::string_view name) {
    std::string out;
    for (const unsigned char c : name) {
        if (std::isalnum(c) != 0) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

bool isGameFile(const RomSystem& system, const std::string& name) {
    const std::string extension = lower(fs::path{name}.extension().string());
    return std::ranges::find(system.extensions, extension) != system.extensions.end();
}

/// Matches one path component against a pattern with at most one `*`.
bool matches(std::string_view pattern, const std::string& name) {
    const std::string wanted = lower(pattern);
    const std::size_t star = wanted.find('*');
    if (star == std::string::npos) {
        return wanted == name;
    }
    const std::string_view head = std::string_view{wanted}.substr(0, star);
    const std::string_view tail = std::string_view{wanted}.substr(star + 1);
    return name.size() >= head.size() + tail.size() && name.starts_with(head) &&
           name.ends_with(tail);
}

/// The file `marker` names inside `folder`, if there is one.
std::optional<fs::path> findMarker(const RomTree& tree, const RomNode& folder,
                                   std::string_view marker) {
    std::vector<RomNode> here{folder};
    std::size_t start = 0;
    while (start <= marker.size()) {
        const std::size_t end = std::min(marker.find('/', start), marker.size());
        const std::string_view part = marker.substr(start, end - start);
        std::vector<RomNode> next;
        for (const RomNode& dir : here) {
            for (RomNode& entry : tree.children(dir.location).value_or(std::vector<RomNode>{})) {
                if (matches(part, lower(entry.name))) {
                    next.push_back(std::move(entry));
                }
            }
        }
        here = std::move(next);
        if (end == marker.size()) {
            break;
        }
        start = end + 1;
    }
    for (const RomNode& found : here) {
        if (!found.folder) {
            return found.location;
        }
    }
    return std::nullopt;
}

/// An update or DLC beside the base game, by the names dumps give them.
bool isAddOn(const RomNode& file) {
    const std::string name = lower(file.name);
    if (name.find("update") != std::string::npos || name.find("dlc") != std::string::npos) {
        return true;
    }
    // A Switch title id not ending in 000 is an update (800) or an add-on.
    for (std::size_t open = name.find('['); open != std::string::npos;
         open = name.find('[', open + 1)) {
        const std::string_view id = std::string_view{name}.substr(open + 1, titleIdDigits);
        if (id.size() == titleIdDigits && open + 1 + titleIdDigits < name.size() &&
            name[open + 1 + titleIdDigits] == ']' &&
            std::ranges::all_of(
                id,
                [](unsigned char c) {
                    return std::isxdigit(c) != 0;
                })) {
            return !id.ends_with("000");
        }
    }
    // A version tag other than [v0] marks an update.
    const std::size_t tag = name.find("[v");
    return tag != std::string::npos && tag + 2 < name.size() && name[tag + 2] != '0';
}

/// The files of `system` under `folder`, `depth` levels down at most.
void collectGames(const RomTree& tree, const RomSystem& system, const fs::path& folder, int depth,
                  std::vector<RomNode>& files) {
    for (RomNode& entry : tree.children(folder).value_or(std::vector<RomNode>{})) {
        if (entry.folder) {
            if (depth > 0) {
                collectGames(tree, system, entry.location, depth - 1, files);
            }
        } else if (isGameFile(system, entry.name)) {
            files.push_back(std::move(entry));
        }
    }
}

constexpr auto systems = std::to_array<RomSystem>({
    {"nes",
     "NES",
     "Nintendo - Nintendo Entertainment System",
     {"nes", "famicom", "nintendoentertainmentsystem"},
     {".nes", ".fds"},
     {}},
    {"snes",
     "Super Nintendo",
     "Nintendo - Super Nintendo Entertainment System",
     {"snes", "superfamicom", "supernintendo", "sfc"},
     {".sfc", ".smc"},
     {}},
    {"gb", "Game Boy", "Nintendo - Game Boy", {"gb", "gameboy"}, {".gb"}, {}},
    {"gbc", "Game Boy Color", "Nintendo - Game Boy Color", {"gbc", "gameboycolor"}, {".gbc"}, {}},
    {"gba",
     "Game Boy Advance",
     "Nintendo - Game Boy Advance",
     {"gba", "gameboyadvance"},
     {".gba"},
     {}},
    {"n64",
     "Nintendo 64",
     "Nintendo - Nintendo 64",
     {"n64", "nintendo64"},
     {".z64", ".n64", ".v64"},
     {}},
    {"nds", "Nintendo DS", "Nintendo - Nintendo DS", {"nds", "ds", "nintendods"}, {".nds"}, {}},
    {"n3ds",
     "Nintendo 3DS",
     "Nintendo - Nintendo 3DS",
     {"3ds", "n3ds", "nintendo3ds"},
     {".3ds", ".cci", ".cxi"},
     {}},
    {"gc",
     "GameCube",
     "Nintendo - GameCube",
     {"gc", "gamecube", "ngc"},
     {".rvz", ".iso", ".gcm", ".gcz", ".ciso"},
     {}},
    {"wii", "Wii", "Nintendo - Wii", {"wii"}, {".rvz", ".iso", ".wbfs", ".wia"}, {}},
    {"wiiu",
     "Wii U",
     "Nintendo - Wii U",
     {"wiiu"},
     {".wux", ".wud", ".wua", ".rpx"},
     {"code/*.rpx"}},
    {"switch",
     "Nintendo Switch",
     "",
     {"switch", "nintendoswitch", "ns"},
     {".nsp", ".xci", ".nsz", ".xcz"},
     {}},
    {"psx",
     "PlayStation",
     "Sony - PlayStation",
     {"psx", "ps1", "playstation", "psxchd", "ps1chd"},
     {".chd", ".cue", ".pbp", ".m3u"},
     {}},
    {"ps2",
     "PlayStation 2",
     "Sony - PlayStation 2",
     {"ps2", "playstation2"},
     {".chd", ".iso", ".cso"},
     {}},
    {"ps3",
     "PlayStation 3",
     "Sony - PlayStation 3",
     {"ps3", "playstation3"},
     {".iso"},
     {"ps3_game/usrdir/eboot.bin"}},
    {"ps4", "PlayStation 4", "Sony - PlayStation 4", {"ps4", "playstation4"}, {}, {"eboot.bin"}},
    {"psp",
     "PSP",
     "Sony - PlayStation Portable",
     {"psp", "playstationportable"},
     {".iso", ".cso"},
     {}},
    {"dreamcast",
     "Dreamcast",
     "Sega - Dreamcast",
     {"dreamcast", "dc"},
     {".chd", ".gdi", ".cdi"},
     {}},
    {"xbox", "Xbox", "Microsoft - Xbox", {"xbox"}, {".iso"}, {}},
    {"xbox360",
     "Xbox 360",
     "Microsoft - Xbox 360",
     {"xbox360", "x360"},
     {".iso", ".xex", ".zar"},
     {}},
    {"arcade", "Arcade", "MAME", {"arcade", "mame", "fbneo"}, {".zip"}, {}},
});

} // namespace

std::span<const RomSystem> romSystems() {
    return systems;
}

const RomSystem* systemForFolder(std::string_view folderName) {
    const std::string key = folderKey(folderName);
    for (const RomSystem& system : romSystems()) {
        if (std::ranges::find(system.folders, key) != system.folders.end()) {
            return &system;
        }
    }
    return nullptr;
}

const RomSystem* systemByKey(std::string_view key) {
    const auto found = std::ranges::find(romSystems(), key, &RomSystem::key);
    return found == romSystems().end() ? nullptr : &*found;
}

std::optional<fs::path> gameFile(const RomTree& tree, const RomSystem& system,
                                 const RomNode& entry) {
    if (!entry.folder) {
        if (isGameFile(system, entry.name)) {
            return entry.location;
        }
        return std::nullopt;
    }
    for (const std::string_view marker : system.markers) {
        if (std::optional<fs::path> found = findMarker(tree, entry, marker)) {
            return found;
        }
    }
    std::vector<RomNode> files;
    collectGames(tree, system, entry.location, folderDepth, files);
    if (files.empty()) {
        return std::nullopt;
    }
    std::vector<RomNode> bases;
    std::ranges::copy_if(files, std::back_inserter(bases), [](const RomNode& file) {
        return !isAddOn(file);
    });
    const std::vector<RomNode>& pool = bases.empty() ? files : bases;
    return std::ranges::max_element(pool, {}, &RomNode::size)->location;
}

std::string systemLabel(std::string_view key) {
    const RomSystem* system = systemByKey(key);
    return system != nullptr ? std::string{system->label} : std::string{key};
}

} // namespace opensu::library::roms
