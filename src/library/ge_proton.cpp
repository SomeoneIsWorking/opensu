#include "ge_proton.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <system_error>

namespace opensu::library::ge_proton {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view namePrefix = "GE-Proton";
constexpr std::size_t sha512HexLength = 128;

#if defined(__aarch64__)
constexpr const char* tarballPattern = R"(GE-Proton[0-9]+-[0-9]+-aarch64\.tar\.gz)";
constexpr const char* checksumPattern = R"(GE-Proton[0-9]+-[0-9]+-aarch64\.sha512sum)";
#else
constexpr const char* tarballPattern = R"(GE-Proton[0-9]+-[0-9]+(-x86_64)?\.tar\.gz)";
constexpr const char* checksumPattern = R"(GE-Proton[0-9]+-[0-9]+(-x86_64)?\.sha512sum)";
#endif

std::optional<int> numberAt(std::string_view& text) {
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end == text.data()) {
        return std::nullopt;
    }
    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    return value;
}

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

bool isHex(std::string_view text) {
    return std::ranges::all_of(text, [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

} // namespace

std::optional<Version> versionOf(std::string_view name) {
    if (!name.starts_with(namePrefix)) {
        return std::nullopt;
    }
    name.remove_prefix(namePrefix.size());
    const std::optional<int> major = numberAt(name);
    if (!major || !name.starts_with('-')) {
        return std::nullopt;
    }
    name.remove_prefix(1);
    const std::optional<int> minor = numberAt(name);
    if (!minor || !name.empty()) {
        return std::nullopt;
    }
    return Version{.major = *major, .minor = *minor};
}

fs::path runnersDir(const fs::path& dataDir) {
    return dataDir / "wine" / "runners";
}

roms::ReleaseSource tarballSource(std::string_view api) {
    return roms::ReleaseSource{.api = std::string{api.empty() ? latestApi : api},
                               .asset = tarballPattern,
                               .downloads = std::string{downloads}};
}

roms::ReleaseSource checksumSource(std::string_view api) {
    return roms::ReleaseSource{.api = std::string{api.empty() ? latestApi : api},
                               .asset = checksumPattern,
                               .downloads = std::string{downloads}};
}

std::optional<std::string> sha512Listed(std::string_view listing, std::string_view file) {
    while (!listing.empty()) {
        const std::size_t end = listing.find('\n');
        const std::string_view line = trimmed(listing.substr(0, end));
        listing = end == std::string_view::npos ? std::string_view{} : listing.substr(end + 1);
        const std::size_t gap = line.find_first_of(" \t");
        if (gap == std::string_view::npos) {
            continue;
        }
        std::string_view named = trimmed(line.substr(gap));
        // sha512sum marks a file read in binary mode with '*'.
        if (named.starts_with('*')) {
            named.remove_prefix(1);
        }
        const std::string_view digest = line.substr(0, gap);
        if (named != file || digest.size() != sha512HexLength || !isHex(digest)) {
            continue;
        }
        std::string lowered{digest};
        std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lowered;
    }
    return std::nullopt;
}

std::vector<Build> installed(const fs::path& runners) {
    std::vector<Build> builds;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{runners, error}) {
        const std::string name = entry.path().filename().string();
        const std::optional<Version> version = versionOf(name);
        std::error_code ec;
        if (!version || !entry.is_directory(ec) ||
            !fs::is_regular_file(entry.path() / "proton", ec) ||
            !fs::is_regular_file(entry.path() / "files" / "bin" / "wine", ec)) {
            continue;
        }
        builds.push_back(Build{.name = name, .version = *version, .dir = entry.path()});
    }
    std::ranges::sort(builds, std::greater{}, &Build::version);
    return builds;
}

bool managed(const fs::path& runners, const fs::path& dir) {
    return !runners.empty() && dir.parent_path() == runners && versionOf(dir.filename().string());
}

std::vector<Build> rotation(const std::vector<Build>& newestFirst,
                            const std::set<fs::path>& inUse) {
    std::vector<Build> removed;
    for (std::size_t at = kept; at < newestFirst.size(); ++at) {
        if (!inUse.contains(newestFirst[at].dir)) {
            removed.push_back(newestFirst[at]);
        }
    }
    return removed;
}

} // namespace opensu::library::ge_proton
