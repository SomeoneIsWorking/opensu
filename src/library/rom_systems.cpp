#include "rom_systems.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <system_error>

namespace iideck::library::roms {
namespace {

namespace fs = std::filesystem;

/// How deep a game folder is searched for its file: Switch dumps nest one level, Wii U and PS3
/// games two.
constexpr int folderDepth = 3;

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

bool isGameFile(const RomSystem& system, const fs::path& file) {
    const std::string extension = lower(file.extension().string());
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
std::optional<fs::path> findMarker(const fs::path& folder, std::string_view marker) {
    std::vector<fs::path> here{folder};
    std::size_t start = 0;
    while (start <= marker.size()) {
        const std::size_t end = std::min(marker.find('/', start), marker.size());
        const std::string_view part = marker.substr(start, end - start);
        std::vector<fs::path> next;
        for (const fs::path& dir : here) {
            std::error_code ec;
            for (const fs::directory_entry& entry : fs::directory_iterator{dir, ec}) {
                if (matches(part, lower(entry.path().filename().string()))) {
                    next.push_back(entry.path());
                }
            }
        }
        here = std::move(next);
        if (end == marker.size()) {
            break;
        }
        start = end + 1;
    }
    for (const fs::path& found : here) {
        std::error_code ec;
        if (fs::is_regular_file(found, ec)) {
            return found;
        }
    }
    return std::nullopt;
}

/// An update or DLC beside the base game, by the names dumps give them.
bool isAddOn(const fs::path& file) {
    const std::string name = lower(file.filename().string());
    if (name.find("update") != std::string::npos || name.find("dlc") != std::string::npos) {
        return true;
    }
    // A version tag other than [v0] marks an update.
    const std::size_t tag = name.find("[v");
    return tag != std::string::npos && tag + 2 < name.size() && name[tag + 2] != '0';
}

std::uintmax_t sizeOf(const fs::path& file) {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    return ec ? 0 : size;
}

const std::vector<RomSystem> systems{
    {"nes", "NES", {"nes", "famicom", "nintendoentertainmentsystem"}, {".nes", ".fds"}, {}},
    {"snes",
     "Super Nintendo",
     {"snes", "superfamicom", "supernintendo", "sfc"},
     {".sfc", ".smc"},
     {}},
    {"gb", "Game Boy", {"gb", "gameboy"}, {".gb"}, {}},
    {"gbc", "Game Boy Color", {"gbc", "gameboycolor"}, {".gbc"}, {}},
    {"gba", "Game Boy Advance", {"gba", "gameboyadvance"}, {".gba"}, {}},
    {"n64", "Nintendo 64", {"n64", "nintendo64"}, {".z64", ".n64", ".v64"}, {}},
    {"nds", "Nintendo DS", {"nds", "ds", "nintendods"}, {".nds"}, {}},
    {"n3ds", "Nintendo 3DS", {"3ds", "n3ds", "nintendo3ds"}, {".3ds", ".cci", ".cxi"}, {}},
    {"gc", "GameCube", {"gc", "gamecube", "ngc"}, {".rvz", ".iso", ".gcm", ".gcz", ".ciso"}, {}},
    {"wii", "Wii", {"wii"}, {".rvz", ".iso", ".wbfs", ".wia"}, {}},
    {"wiiu", "Wii U", {"wiiu"}, {".wux", ".wud", ".wua", ".rpx"}, {"code/*.rpx"}},
    {"switch",
     "Nintendo Switch",
     {"switch", "nintendoswitch", "ns"},
     {".nsp", ".xci", ".nsz", ".xcz"},
     {}},
    {"psx",
     "PlayStation",
     {"psx", "ps1", "playstation", "psxchd", "ps1chd"},
     {".chd", ".cue", ".pbp", ".m3u"},
     {}},
    {"ps2", "PlayStation 2", {"ps2", "playstation2"}, {".chd", ".iso", ".cso"}, {}},
    {"ps3", "PlayStation 3", {"ps3", "playstation3"}, {".iso"}, {"ps3_game/usrdir/eboot.bin"}},
    {"ps4", "PlayStation 4", {"ps4", "playstation4"}, {}, {"eboot.bin"}},
    {"psp", "PSP", {"psp", "playstationportable"}, {".iso", ".cso"}, {}},
    {"xbox", "Xbox", {"xbox"}, {".iso"}, {}},
    {"xbox360", "Xbox 360", {"xbox360", "x360"}, {".iso", ".xex", ".zar"}, {}},
    {"arcade", "Arcade", {"arcade", "mame", "fbneo"}, {".zip"}, {}},
};

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

std::optional<fs::path> gameFile(const RomSystem& system, const fs::path& entry) {
    std::error_code ec;
    if (fs::is_regular_file(entry, ec)) {
        if (isGameFile(system, entry)) {
            return entry;
        }
        return std::nullopt;
    }
    if (!fs::is_directory(entry, ec)) {
        return std::nullopt;
    }
    for (const std::string_view marker : system.markers) {
        if (std::optional<fs::path> found = findMarker(entry, marker)) {
            return found;
        }
    }
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it{entry, ec}, end; it != end; it.increment(ec)) {
        if (it.depth() >= folderDepth) {
            it.disable_recursion_pending();
        }
        if (it->is_regular_file(ec) && isGameFile(system, it->path())) {
            files.push_back(it->path());
        }
    }
    if (files.empty()) {
        return std::nullopt;
    }
    std::vector<fs::path> bases;
    std::ranges::copy_if(files, std::back_inserter(bases), [](const fs::path& file) {
        return !isAddOn(file);
    });
    const std::vector<fs::path>& pool = bases.empty() ? files : bases;
    return *std::ranges::max_element(pool, {}, sizeOf);
}

} // namespace iideck::library::roms
