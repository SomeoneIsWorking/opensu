#include "lutris_wine.hpp"

#include <array>
#include <fstream>
#include <system_error>
#include <utility>

namespace opensu::library::lutris {
namespace {

namespace fs = std::filesystem;

/// Lutris's `WINE_PATHS`.
constexpr std::array<std::pair<std::string_view, std::string_view>, 4> systemWines{{
    {"winehq-devel", "/opt/wine-devel/bin/wine"},
    {"winehq-staging", "/opt/wine-staging/bin/wine"},
    {"wine-development", "/usr/lib/wine-development/wine"},
    {"system", "wine"},
}};

std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r");
    text = text.substr(first, last - first + 1);
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') &&
        text.back() == text.front()) {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

} // namespace

fs::path dataDir(const fs::path& xdgDataHome) {
    return xdgDataHome / "lutris";
}

fs::path wineDir(const fs::path& lutrisData) {
    return lutrisData / "runners" / "wine";
}

std::string defaultWineVersion(const fs::path& lutrisData) {
    std::ifstream file{lutrisData / "runners" / "wine.yml"};
    bool inWine = false;
    for (std::string line; std::getline(file, line);) {
        const std::string_view text{line};
        if (!text.empty() && text.front() != ' ' && text.front() != '\t') {
            inWine = trimmed(text) == "wine:";
            continue;
        }
        const std::string_view entry = trimmed(text);
        if (inWine && entry.starts_with("version:")) {
            const std::string_view version =
                trimmed(entry.substr(std::string_view{"version:"}.size()));
            if (!version.empty()) {
                return std::string{version};
            }
        }
    }
    return std::string{geProtonLatest};
}

std::optional<fs::path> systemWineOf(std::string_view version) {
    for (const auto& [name, binary] : systemWines) {
        if (name == version) {
            return fs::path{binary};
        }
    }
    return std::nullopt;
}

fs::path umuRun(const fs::path& lutrisData) {
    const fs::path run = lutrisData / "runtime" / "umu" / "umu-run";
    std::error_code error;
    return fs::is_regular_file(run, error) ? run : fs::path{};
}

} // namespace opensu::library::lutris
