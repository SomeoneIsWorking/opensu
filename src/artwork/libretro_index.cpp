#include "libretro_index.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <limits>

#include "artwork_store.hpp"
#include "lucent/log.h"
#include "rom_titles.hpp"

namespace opensu::artwork {
namespace {

constexpr std::string_view pngSuffix = ".png";

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The name split into its title and its bracketed tags.
struct Parsed {
    /// Letters and digits of the title, lower-case, with "the" and "and" dropped, so "Legend of
    /// Zelda, The - Wind Waker" and "The Legend of Zelda Wind Waker" agree, and "Kirby & The"
    /// with "Kirby _ the", where the file name lost its ampersand.
    std::string title;
    std::vector<std::string> tags;
};

/// GoodTools' one-letter regions, as No-Intro names them.
std::string region(std::string tag) {
    if (tag == "u") {
        return "usa";
    }
    if (tag == "e") {
        return "europe";
    }
    if (tag == "j") {
        return "japan";
    }
    if (tag == "w") {
        return "world";
    }
    return tag;
}

/// A title's words: letters and digits, lower-case, without "the" and "and".
std::vector<std::string> wordsOf(std::string_view title) {
    std::vector<std::string> words;
    std::string word;
    const auto flush = [&words, &word] {
        if (!word.empty() && word != "the" && word != "and") {
            words.push_back(word);
        }
        word.clear();
    };
    for (const char c : title) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) != 0) {
            word.push_back(static_cast<char>(std::tolower(u)));
        } else {
            flush();
        }
    }
    flush();
    return words;
}

Parsed parse(std::string_view whole) {
    Parsed parsed;
    const std::string_view name = library::roms::withoutReleaseNumber(whole);
    const std::size_t cut = std::min(name.find(" ("), name.find(" ["));
    for (const std::string& word : wordsOf(name.substr(0, cut))) {
        parsed.title += word;
    }
    if (cut == std::string_view::npos) {
        return parsed;
    }
    for (std::size_t open = name.find_first_of("([", cut); open != std::string_view::npos;
         open = name.find_first_of("([", open + 1)) {
        const std::size_t close = name.find_first_of(")]", open);
        if (close == std::string_view::npos) {
            break;
        }
        // "(Europe, Australia)" and "(En,Fr,De)" hold several tags each.
        const std::string_view group = name.substr(open + 1, close - open - 1);
        std::size_t start = 0;
        while (start <= group.size()) {
            const std::size_t comma = std::min(group.find(',', start), group.size());
            std::string_view tag = group.substr(start, comma - start);
            while (!tag.empty() && tag.front() == ' ') {
                tag.remove_prefix(1);
            }
            if (!tag.empty()) {
                parsed.tags.push_back(region(lower(tag)));
            }
            start = comma + 1;
        }
        open = close;
    }
    return parsed;
}

bool hasTag(const Parsed& parsed, std::string_view tag) {
    return std::ranges::find(parsed.tags, tag) != parsed.tags.end();
}

constexpr std::array<std::string_view, 4> preferredRegions{"usa", "world", "europe", "japan"};
constexpr std::array<std::string_view, 5> unreleased{"beta", "demo", "proto", "sample", "kiosk"};
/// Releases that are not the game a title names.
constexpr std::array<std::string_view, 5> notTheGame{"hack", "pirate", "unl", "aftermarket",
                                                     "homebrew"};
/// The fewest words a title needs to be told from another game's.
constexpr std::size_t fewestConfidentWords = 2;

bool isUnreleased(const Parsed& parsed) {
    return std::ranges::any_of(parsed.tags, [](const std::string& tag) {
        return std::ranges::any_of(unreleased, [&tag](std::string_view word) {
            return tag.starts_with(word);
        });
    });
}

/// Lower is better: the ROM's own region, then the preferred order, then fewer tags.
std::size_t rank(const Parsed& rom, const Parsed& entry) {
    std::size_t region = preferredRegions.size() + 1;
    if (std::ranges::any_of(rom.tags, [&entry](const std::string& tag) {
            return hasTag(entry, tag) &&
                   std::ranges::find(preferredRegions, tag) != preferredRegions.end();
        })) {
        region = 0;
    } else {
        for (std::size_t i = 0; i < preferredRegions.size(); ++i) {
            if (hasTag(entry, preferredRegions[i])) {
                region = i + 1;
                break;
            }
        }
    }
    return region * 100 + entry.tags.size();
}

} // namespace

std::vector<std::string> parseIndex(std::string_view html) {
    std::vector<std::string> names;
    constexpr std::string_view marker = "href=\"";
    for (std::size_t at = html.find(marker); at != std::string_view::npos;
         at = html.find(marker, at)) {
        at += marker.size();
        const std::size_t end = html.find('"', at);
        if (end == std::string_view::npos) {
            break;
        }
        const std::string_view link = html.substr(at, end - at);
        if (link.size() > pngSuffix.size() && link.ends_with(pngSuffix) &&
            link.find('/') == std::string_view::npos) {
            names.push_back(net::percentDecode(link.substr(0, link.size() - pngSuffix.size())));
        }
        at = end;
    }
    return names;
}

std::string thumbnailName(std::string_view name) {
    std::string out{name};
    for (char& c : out) {
        if (std::string_view{"&*/:`<>?\\|\""}.find(c) != std::string_view::npos) {
            c = '_';
        }
    }
    return out;
}

std::optional<std::string> bestMatch(std::string_view romName, std::span<const std::string> index) {
    std::string exact = thumbnailName(romName);
    if (std::ranges::find(index, exact) != index.end()) {
        return exact;
    }
    const Parsed rom = parse(romName);
    if (rom.title.empty()) {
        return std::nullopt;
    }
    const bool romUnreleased = isUnreleased(rom);
    std::optional<std::string> best;
    std::size_t bestRank = std::numeric_limits<std::size_t>::max();
    for (const std::string& name : index) {
        const Parsed entry = parse(name);
        if (entry.title != rom.title || (isUnreleased(entry) && !romUnreleased)) {
            continue;
        }
        const std::size_t score = rank(rom, entry);
        if (score < bestRank) {
            bestRank = score;
            best = name;
        }
    }
    return best;
}

std::optional<std::string> confidentMatch(std::string_view title,
                                          std::span<const std::string> index) {
    if (wordsOf(title).size() < fewestConfidentWords ||
        title.find_first_of("([") != std::string_view::npos) {
        return std::nullopt;
    }
    const std::string wanted = parse(title).title;
    std::vector<std::string> same;
    for (const std::string& name : index) {
        const Parsed entry = parse(name);
        const bool barred =
            isUnreleased(entry) || std::ranges::any_of(entry.tags, [](const std::string& tag) {
                return std::ranges::any_of(notTheGame, [&tag](std::string_view word) {
                    return tag.starts_with(word);
                });
            });
        if (entry.title == wanted && !barred) {
            same.push_back(name);
        }
    }
    return bestMatch(title, same);
}

std::vector<std::string> namesMatching(std::string_view term, std::span<const std::string> index,
                                       std::size_t most) {
    const std::vector<std::string> words = wordsOf(term);
    std::vector<std::string> found;
    if (words.empty()) {
        return found;
    }
    for (const std::string& name : index) {
        if (found.size() == most) {
            break;
        }
        const std::string title = parse(name).title;
        if (std::ranges::all_of(words, [&title](const std::string& word) {
                return title.find(word) != std::string::npos;
            })) {
            found.push_back(name);
        }
    }
    return found;
}

std::string encodeSegment(std::string_view segment) {
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string out;
    for (const char c : segment) {
        const auto u = static_cast<unsigned char>(c);
        // Unreserved characters and the sub-delimiters libretro's own links leave as they are.
        if (std::isalnum(u) != 0 ||
            std::string_view{"-_.~()!',"}.find(c) != std::string_view::npos) {
            out.push_back(c);
        } else {
            out.push_back('%');
            out.push_back(hex[u >> 4U]);
            out.push_back(hex[u & 0x0FU]);
        }
    }
    return out;
}

std::string boxArtUrl(std::string_view base, const library::roms::RomSystem& system,
                      std::string_view name) {
    return std::string{base} + "/" + encodeSegment(system.libretroName) + "/Named_Boxarts/" +
           encodeSegment(name) + ".png";
}

net::Result<std::vector<std::string>>
boxArtNames(const std::stop_token& stop, const net::WebClient& web, const ArtworkStore& store,
            const library::roms::RomSystem& system, std::string_view base) {
    if (std::optional<std::vector<std::string>> kept =
            store.index(system.key, ArtworkStore::Clock::now())) {
        return std::move(*kept);
    }
    const std::string url =
        std::string{base} + "/" + encodeSegment(system.libretroName) + "/Named_Boxarts/";
    const net::Result<std::string> html = web.get(stop, url);
    if (!html) {
        return html.as<std::vector<std::string>>(url + ": ");
    }
    std::vector<std::string> names = parseIndex(*html);
    store.saveIndex(system.key, names);
    lucent::info("artwork", "{} box art names listed for {}", names.size(), system.libretroName);
    return names;
}

} // namespace opensu::artwork
