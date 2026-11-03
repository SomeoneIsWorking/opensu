#include "lutris_wine.hpp"

#include <array>
#include <system_error>
#include <utility>

#include "lutris_config.hpp"

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

} // namespace

fs::path dataDir(const fs::path& xdgDataHome) {
    return xdgDataHome / "lutris";
}

fs::path wineDir(const fs::path& lutrisData) {
    return lutrisData / "runners" / "wine";
}

std::string defaultWineVersion(const fs::path& lutrisData) {
    std::string version = configValue(lutrisData / "runners" / "wine.yml", {"wine", "version"});
    return version.empty() ? std::string{geProtonLatest} : version;
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
