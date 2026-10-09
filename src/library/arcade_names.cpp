#include "arcade_names.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>

#include "fileio/atomic_write.hpp"

namespace opensu::library::roms {
namespace {

constexpr std::string_view nameKey = "name \"";
constexpr std::string_view romKey = "rom ( name ";
constexpr std::string_view zipSuffix = ".zip";

std::string_view trimmed(std::string_view line) {
    while (!line.empty() && (line.front() == '\t' || line.front() == ' ')) {
        line.remove_prefix(1);
    }
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.remove_suffix(1);
    }
    return line;
}

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The short name in a `rom ( name sf2.zip size ...` line; empty when it is not a zip's.
std::string shortName(std::string_view line) {
    if (!line.starts_with(romKey)) {
        return {};
    }
    line.remove_prefix(romKey.size());
    const std::string_view file = line.substr(0, line.find(' '));
    if (!file.ends_with(zipSuffix) || file.size() == zipSuffix.size()) {
        return {};
    }
    return lower(file.substr(0, file.size() - zipSuffix.size()));
}

} // namespace

NameMap parseDat(std::string_view text) {
    NameMap names;
    std::string description;
    bool inGame = false;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string_view line = trimmed(text.substr(at, end - at));
        at = end + 1;
        if (line == "game (") {
            inGame = true;
            description.clear();
        } else if (line == ")") {
            inGame = false;
        } else if (inGame && description.empty() && line.starts_with(nameKey) && line.size() > 6) {
            description =
                std::string{line.substr(nameKey.size(), line.size() - nameKey.size() - 1)};
        } else if (inGame && !description.empty()) {
            if (std::string key = shortName(line); !key.empty()) {
                names.try_emplace(std::move(key), description);
            }
        }
    }
    return names;
}

NameDb::NameDb(std::filesystem::path dir) : dir_{std::move(dir)} {
}

NameDb NameDb::under(const std::filesystem::path& cacheDir) {
    return NameDb{cacheDir / "names"};
}

std::filesystem::path NameDb::file() const {
    return dir_ / ("arcade-" + std::string{libretroDatabaseRevision.substr(0, 12)} + ".tsv");
}

bool NameDb::cached() const {
    std::error_code ec;
    return enabled() && std::filesystem::is_regular_file(file(), ec);
}

NameMap NameDb::load() const {
    NameMap names;
    if (!enabled()) {
        return names;
    }
    std::ifstream in{file(), std::ios::binary};
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t tab = line.find('\t');
        if (tab != std::string::npos && tab > 0) {
            names.emplace(line.substr(0, tab), line.substr(tab + 1));
        }
    }
    return names;
}

bool NameDb::save(const NameMap& names, std::string& error) const {
    if (!enabled()) {
        error = "no cache folder for names";
        return false;
    }
    std::vector<const NameMap::value_type*> rows;
    rows.reserve(names.size());
    for (const NameMap::value_type& row : names) {
        rows.push_back(&row);
    }
    std::ranges::sort(rows, [](const auto* a, const auto* b) {
        return a->first < b->first;
    });
    std::string bytes;
    for (const NameMap::value_type* row : rows) {
        bytes += row->first + '\t' + row->second + '\n';
    }
    return fileio::writeWhole(file(), bytes, error);
}

} // namespace opensu::library::roms
