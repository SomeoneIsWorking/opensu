#include "libretro_index.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <limits>

#include "rom_titles.hpp"

namespace opensu::artwork {
namespace {

constexpr std::string_view pngSuffix = ".png";

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower >= 'a' && lower <= 'f') {
        return lower - 'a' + 10;
    }
    return -1;
}

std::string decode(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int high = hexValue(text[i + 1]);
            const int low = hexValue(text[i + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<char>(high * 16 + low));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

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

Parsed parse(std::string_view whole) {
    Parsed parsed;
    const std::string_view name = library::roms::withoutReleaseNumber(whole);
    const std::size_t cut = std::min(name.find(" ("), name.find(" ["));
    const std::string_view title = name.substr(0, cut);
    std::string word;
    const auto flush = [&parsed, &word] {
        if (!word.empty() && word != "the" && word != "and") {
            parsed.title += word;
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
            names.push_back(decode(link.substr(0, link.size() - pngSuffix.size())));
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

} // namespace opensu::artwork
