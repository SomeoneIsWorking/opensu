#include "catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ranges>
#include <string_view>
#include <vector>

namespace iideck::library {
namespace {

/// Splits a colon-separated path list, expanding a leading `~/`.
std::vector<std::filesystem::path> splitPaths(const char* raw)
{
    std::vector<std::filesystem::path> out;
    if (raw == nullptr) {
        return out;
    }
    const std::string_view text{raw};
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find(':', start), text.size());
        std::string_view piece = text.substr(start, end - start);
        if (!piece.empty()) {
            if (piece.starts_with("~/")) {
                if (const char* home = std::getenv("HOME"); home != nullptr) {
                    out.emplace_back(std::filesystem::path{home} / piece.substr(2));
                    piece = {};
                }
            }
            if (!piece.empty()) {
                out.emplace_back(piece);
            }
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }
    return out;
}

/// Splits a shell-like command into words, honouring double quotes so an
/// argument can contain spaces.
std::vector<std::string> splitWords(std::string_view text)
{
    std::vector<std::string> out;
    std::string current;
    bool inQuotes = false;
    for (const char c : text) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && (c == ' ' || c == '\t')) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

/// Reads "SYSTEM=program arg;SYSTEM2=program". Splitting on '=' and then on
/// whitespace keeps a command's own arguments unambiguous.
std::map<std::string, std::vector<std::string>, std::less<>> parseEmulators(const char* raw)
{
    std::map<std::string, std::vector<std::string>, std::less<>> out;
    if (raw == nullptr) {
        return out;
    }
    std::string_view text{raw};
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find(';', start), text.size());
        std::string_view entry = text.substr(start, end - start);
        if (!entry.empty()) {
            const std::size_t equals = entry.find('=');
            if (equals != std::string_view::npos) {
                std::string name{entry.substr(0, equals)};
                std::ranges::transform(name, name.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                std::vector<std::string> words = splitWords(entry.substr(equals + 1));
                if (!name.empty() && !words.empty()) {
                    out.emplace(std::move(name), std::move(words));
                }
            }
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }
    return out;
}

/// The extension-to-system map. Deliberately a small built-in set: a real
/// ES-DE systems configuration replaces it.
const std::map<std::string, std::string, std::less<>>& defaultExtensions()
{
    static const std::map<std::string, std::string, std::less<>> extensions{
        {".NES", "NES"},     {".SFC", "SNES"},     {".SMC", "SNES"},
        {".MD", "Genesis"},  {".GEN", "Genesis"},  {".GBA", "Game Boy Advance"},
        {".GB", "Game Boy"}, {".GBC", "Game Boy Color"},
        {".PS1", "PlayStation"}, {".PBP", "PlayStation"}, {".CUE", "PlayStation"},
        {".CHD", "PlayStation"}, {".ISO", "PlayStation"}, {".BIN", "PlayStation"},
        {".Z64", "Nintendo 64"}, {".N64", "Nintendo 64"},
        {".SMD", "Master System"}, {".GG", "Game Gear"},
        {".MDX", "Mega Drive"},
    };
    return extensions;
}

} // namespace

Config readConfig()
{
    Config config;
    config.steamRoots = splitPaths(std::getenv("IIDECK_STEAM_ROOTS"));
    config.romRoots = splitPaths(std::getenv("IIDECK_ROM_ROOTS"));
    config.emulators = parseEmulators(std::getenv("IIDECK_EMULATORS"));
    config.extensions = defaultExtensions();
    return config;
}

Catalog makeCatalog(const Config& config)
{
    Catalog catalog;
    catalog.add(std::make_unique<steam::Provider>(steam::Library::discover(config.steamRoots)));
    catalog.add(std::make_unique<epic::Provider>());
    catalog.add(std::make_unique<gog::Provider>());
    catalog.add(std::make_unique<roms::Provider>(config.romRoots, config.extensions, config.emulators));
    return catalog;
}

} // namespace iideck::library