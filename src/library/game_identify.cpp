#include "game_identify.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "net/web_client.hpp"
#include "titles.hpp"

namespace opensu::library::identify {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;

/// Folders above the file that are searched for clues.
constexpr int mostFolders = 4;
/// The most clues looked up in the store.
constexpr std::size_t mostSearches = 6;
/// The most bytes of a resource section read.
constexpr std::uint32_t mostResourceBytes = 32U << 20U;
/// PE's resource type of a version resource.
constexpr std::uint32_t versionResource = 16;

/// Names of engines' placeholders, as `titleKey`s.
constexpr std::array<std::string_view, 10> placeholderNames{
    "bootstrappackagedgame", "unrealgame", "ue4game",  "ue5game",
    "unityplayer",           "unity",      "launcher", "game",
    "application",           "setup"};
/// File names that say nothing about the game.
constexpr std::array<std::string_view, 8> launcherNames{"start", "apprun", "launcher", "launch",
                                                        "game",  "run",    "play",     "setup"};
/// Build words at the end of a file name.
constexpr std::array<std::string_view, 15> buildWords{
    "steam", "shipping", "win64", "win32", "x64",  "x86",    "x8664", "amd64",
    "linux", "dx11",     "dx12",  "gog",   "epic", "retail", "egs"};
/// Folders inside a game that say nothing about it, as `titleKey`s.
constexpr std::array<std::string_view, 13> innerFolders{
    "bin", "bin32", "bin64", "binaries", "win32",  "win64",  "x64",
    "x86", "x8664", "game",  "system",   "retail", "release"};
/// Folders that hold games rather than being one, as `titleKey`s: the walk up stops there.
constexpr std::array<std::string_view, 12> libraryFolders{
    "games",        "appimages",       "applications", "apps",   "downloads", "desktop",
    "programfiles", "programfilesx86", "drivec",       "common", "steamapps", "opt"};

template <std::size_t N>
bool among(const std::array<std::string_view, N>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::uint32_t le(const Bytes& bytes, std::size_t at, std::size_t width) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width && at + i < bytes.size(); ++i) {
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    }
    return value;
}

Bytes readAt(std::ifstream& in, std::uint64_t offset, std::uint32_t size) {
    Bytes bytes(size);
    in.seekg(static_cast<std::streamoff>(offset));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    in.clear();
    return bytes;
}

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code < 0x80U) {
        out += static_cast<char>(code);
    } else if (code < 0x800U) {
        out += static_cast<char>(0xC0U | (code >> 6U));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else if (code < 0x10000U) {
        out += static_cast<char>(0xE0U | (code >> 12U));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (code >> 18U));
        out += static_cast<char>(0x80U | ((code >> 12U) & 0x3FU));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    }
}

/// The UTF-16LE text at `at`, to its terminator or `end`.
std::string utf16At(const Bytes& bytes, std::size_t at, std::size_t end) {
    std::string out;
    for (std::size_t i = at; i + 1 < std::min(end, bytes.size()); i += 2) {
        std::uint32_t unit = le(bytes, i, 2);
        if (unit == 0) {
            break;
        }
        if (unit >= 0xD800U && unit < 0xDC00U && i + 3 < bytes.size()) {
            const std::uint32_t low = le(bytes, i + 2, 2);
            unit = 0x10000U + ((unit - 0xD800U) << 10U) + (low - 0xDC00U);
            i += 2;
        }
        appendUtf8(out, unit);
    }
    return out;
}

/// The value of String `key` in a VS_VERSIONINFO block.
std::string versionString(const Bytes& block, std::string_view key) {
    Bytes pattern;
    for (const char c : key) {
        pattern.push_back(static_cast<unsigned char>(c));
        pattern.push_back(0);
    }
    pattern.push_back(0);
    pattern.push_back(0);
    const auto found = std::ranges::search(block, pattern);
    if (found.empty()) {
        return {};
    }
    const auto keyAt = static_cast<std::size_t>(found.begin() - block.begin());
    // A String is wLength, wValueLength, wType, then its key; the value starts 32-bit aligned.
    if (keyAt < 6) {
        return {};
    }
    const std::size_t end = keyAt - 6 + le(block, keyAt - 6, 2);
    const std::size_t valueAt = (keyAt + pattern.size() + 3) & ~static_cast<std::size_t>(3);
    std::string value = utf16At(block, valueAt, end);
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    return value;
}

/// The data of the first resource of type `type` in a resource section at `rva`.
Bytes resourceOf(const Bytes& section, std::uint32_t rva, std::uint32_t type) {
    std::uint32_t directory = 0;
    for (int level = 0; level < 3; ++level) {
        const std::uint32_t entries =
            le(section, directory + 12, 2) + le(section, directory + 14, 2);
        std::uint32_t next = 0;
        bool found = false;
        for (std::uint32_t i = 0; i < entries; ++i) {
            const std::size_t entry = directory + 16 + (static_cast<std::size_t>(i) * 8);
            if (level == 0 && le(section, entry, 4) != type) {
                continue;
            }
            next = le(section, entry + 4, 4);
            found = true;
            break;
        }
        if (!found) {
            return {};
        }
        if (level < 2) {
            if ((next & 0x80000000U) == 0) {
                return {};
            }
            directory = next & 0x7FFFFFFFU;
        } else {
            const std::uint32_t dataRva = le(section, next, 4);
            const std::uint32_t size = le(section, next + 4, 4);
            if (dataRva < rva || dataRva - rva + size > section.size()) {
                return {};
            }
            const auto first = section.begin() + static_cast<std::ptrdiff_t>(dataRva - rva);
            return Bytes{first, first + static_cast<std::ptrdiff_t>(size)};
        }
    }
    return {};
}

/// An .exe's VS_VERSIONINFO block; empty when it has none.
Bytes versionBlock(const fs::path& exe) {
    std::ifstream in{exe, std::ios::binary};
    if (!in) {
        return {};
    }
    const Bytes dos = readAt(in, 0, 64);
    if (dos.size() < 64 || dos[0] != 'M' || dos[1] != 'Z') {
        return {};
    }
    const std::uint32_t pe = le(dos, 0x3C, 4);
    const Bytes headers = readAt(in, pe, 4096);
    if (headers.size() < 24 || le(headers, 0, 4) != 0x00004550U) {
        return {};
    }
    const std::uint32_t sections = le(headers, 6, 2);
    const std::uint32_t optionalSize = le(headers, 20, 2);
    const std::size_t optional = 24;
    const std::uint32_t magic = le(headers, optional, 2);
    // The resource directory is data directory 2.
    const std::size_t directories = optional + (magic == 0x20BU ? 112 : 96);
    const std::uint32_t resourceRva = le(headers, directories + 16, 4);
    if (resourceRva == 0) {
        return {};
    }
    for (std::uint32_t i = 0; i < sections; ++i) {
        const std::size_t header = optional + optionalSize + (static_cast<std::size_t>(i) * 40);
        const std::uint32_t virtualAddress = le(headers, header + 12, 4);
        const std::uint32_t rawSize = le(headers, header + 16, 4);
        const std::uint32_t rawAt = le(headers, header + 20, 4);
        if (resourceRva < virtualAddress || resourceRva >= virtualAddress + rawSize) {
            continue;
        }
        const std::uint32_t skip = resourceRva - virtualAddress;
        const Bytes section = readAt(in, static_cast<std::uint64_t>(rawAt) + skip,
                                     std::min(rawSize - skip, mostResourceBytes));
        return resourceOf(section, resourceRva, versionResource);
    }
    return {};
}

/// `name` without the trademark signs a store puts in titles.
std::string unmarked(std::string name) {
    for (const std::string_view mark : {"\xE2\x84\xA2", "\xC2\xAE", "\xC2\xA9"}) {
        for (std::size_t at = name.find(mark); at != std::string::npos; at = name.find(mark)) {
            name.erase(at, mark.size());
        }
    }
    return name;
}

std::size_t depthOf(const fs::path& dir) {
    return static_cast<std::size_t>(
        std::distance(dir.relative_path().begin(), dir.relative_path().end()));
}

/// The `Name=` of a .desktop entry in `dir` whose `Exec=` names `file`.
std::string desktopName(const fs::path& dir, const fs::path& file) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, error}) {
        if (lowered(entry.path().extension().string()) != ".desktop") {
            continue;
        }
        std::ifstream in{entry.path()};
        std::string name;
        bool starts = false;
        for (std::string line; std::getline(in, line);) {
            if (line.starts_with("Name=") && name.empty()) {
                name = line.substr(5);
            } else if (line.starts_with("Exec=")) {
                starts = line.find(file.filename().string()) != std::string::npos;
            }
        }
        if (starts && !name.empty()) {
            return name;
        }
    }
    return {};
}

/// The `name` of a GOG `goggame-*.info` in `dir`.
std::string gogName(const fs::path& dir) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, error}) {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with("goggame-") || !name.ends_with(".info")) {
            continue;
        }
        std::ifstream in{entry.path()};
        const nlohmann::json info = nlohmann::json::parse(in, nullptr, false);
        if (info.is_object() && info.contains("name") && info["name"].is_string()) {
            return info["name"].get<std::string>();
        }
    }
    return {};
}

/// The app id in `dir/steam_appid.txt`.
std::string steamAppIdIn(const fs::path& dir) {
    std::ifstream in{dir / "steam_appid.txt"};
    std::string id;
    in >> id;
    const bool digits = !id.empty() && std::ranges::all_of(id, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    return digits ? id : std::string{};
}

void addClue(std::vector<Clue>& names, std::string name, const char* from) {
    const std::string key = titleKey(name);
    if (key.empty() || std::ranges::any_of(names, [&key](const Clue& clue) {
            return titleKey(clue.name) == key;
        })) {
        return;
    }
    names.push_back({std::move(name), from});
}

} // namespace

std::string versionName(const fs::path& exe) {
    const Bytes block = versionBlock(exe);
    if (block.empty()) {
        return {};
    }
    for (const std::string_view key : {"ProductName", "FileDescription"}) {
        std::string name = versionString(block, key);
        if (!titleKey(name).empty() && !among(placeholderNames, titleKey(name))) {
            return name;
        }
    }
    return {};
}

std::string fileTitle(const fs::path& file) {
    const std::string stem = file.stem().string();
    if (among(launcherNames, lowered(stem))) {
        return {};
    }
    std::vector<std::string> words;
    std::string word;
    for (const char c : stem + " ") {
        if (c == '_' || c == '-' || c == ' ' || c == '.') {
            if (!word.empty()) {
                words.push_back(std::exchange(word, {}));
            }
        } else {
            word += c;
        }
    }
    while (words.size() > 1 && among(buildWords, titleKey(words.back()))) {
        words.pop_back();
    }
    const bool lower = std::ranges::none_of(stem, [](unsigned char c) {
        return std::isupper(c) != 0;
    });
    std::string title;
    for (std::string& each : words) {
        if (lower && !each.empty()) {
            each.front() =
                static_cast<char>(std::toupper(static_cast<unsigned char>(each.front())));
        }
        title += (title.empty() ? "" : " ") + each;
    }
    return title;
}

Clues cluesOf(const fs::path& file) {
    Clues clues;
    std::vector<Clue> folders;
    addClue(clues.names, desktopName(file.parent_path(), file), "its .desktop entry");
    fs::path dir = file.parent_path();
    for (int level = 0; level < mostFolders && depthOf(dir) > 2; ++level) {
        if (clues.steamAppId.empty()) {
            clues.steamAppId = steamAppIdIn(dir);
        }
        const std::string name = dir.filename().string();
        if (among(libraryFolders, titleKey(name))) {
            break;
        }
        addClue(clues.names, gogName(dir), "GOG's game info");
        if (!among(innerFolders, titleKey(name))) {
            addClue(folders, name, "the folder's name");
        }
        dir = dir.parent_path();
    }
    for (Clue& folder : folders) {
        addClue(clues.names, std::move(folder.name), "the folder's name");
    }
    if (lowered(file.extension().string()) == ".exe") {
        addClue(clues.names, versionName(file), "the program's version info");
    }
    addClue(clues.names, fileTitle(file), "the file's name");
    return clues;
}

Guess identify(const fs::path& file, const StoreSearch& search) {
    const Clues clues = cluesOf(file);
    Guess guess{.title = clues.names.empty() ? file.stem().string() : clues.names.front().name,
                .kind = own::kindOf(file),
                .steamAppId = clues.steamAppId,
                .from = clues.names.empty() ? "the file's name" : clues.names.front().from};
    std::size_t searched = 0;
    for (const Clue& clue : clues.names) {
        if (searched++ == mostSearches) {
            break;
        }
        const std::vector<StoreApp> apps = search(clue.name);
        const auto match = std::ranges::find_if(apps, [&](const StoreApp& app) {
            return clues.steamAppId.empty() ? titleKey(app.title) == titleKey(clue.name)
                                            : app.appId == clues.steamAppId;
        });
        if (match != apps.end()) {
            guess.title = unmarked(match->title);
            guess.steamAppId = match->appId;
            guess.from = "Steam's store, from " + clue.from;
            break;
        }
    }
    return guess;
}

std::vector<StoreApp> parseStoreSearch(std::string_view json) {
    const nlohmann::json answer = nlohmann::json::parse(json, nullptr, false);
    std::vector<StoreApp> apps;
    if (!answer.is_object() || !answer.contains("items") || !answer["items"].is_array()) {
        return apps;
    }
    for (const nlohmann::json& item : answer["items"]) {
        if (!item.is_object() || item.value("type", "") != "app" || !item.contains("id") ||
            !item["id"].is_number_unsigned() || !item.contains("name") ||
            !item["name"].is_string()) {
            continue;
        }
        apps.push_back(
            {std::to_string(item["id"].get<std::uint64_t>()), item["name"].get<std::string>()});
    }
    return apps;
}

std::string storeSearchUrl(std::string_view name) {
    std::string term;
    for (const char c : name) {
        const auto byte = static_cast<unsigned char>(c);
        const bool kept = byte >= 0x80 || std::isalnum(byte) != 0;
        if (kept) {
            term += c;
        } else if (!term.empty() && term.back() != ' ') {
            term += ' ';
        }
    }
    while (!term.empty() && term.back() == ' ') {
        term.pop_back();
    }
    return "https://store.steampowered.com/api/storesearch/?term=" + net::percentEncode(term) +
           "&l=english&cc=US";
}

} // namespace opensu::library::identify
