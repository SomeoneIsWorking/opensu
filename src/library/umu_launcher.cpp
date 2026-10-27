#include "umu_launcher.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

namespace opensu::library::umu {
namespace {

namespace fs = std::filesystem;

/// `1.4.4` as numbers, so 1.10 sorts after 1.9.
std::vector<int> partsOf(std::string_view version) {
    std::vector<int> parts;
    while (!version.empty()) {
        int value = 0;
        const auto [end, error] =
            std::from_chars(version.data(), version.data() + version.size(), value);
        parts.push_back(error == std::errc{} ? value : -1);
        const std::size_t dot = version.find('.');
        version = dot == std::string_view::npos ? std::string_view{} : version.substr(dot + 1);
    }
    return parts;
}

} // namespace

roms::ReleaseSource zipappSource() {
    return roms::ReleaseSource{.api = std::string{latestApi},
                               .asset = R"(umu-launcher-[0-9.]+-zipapp\.tar)",
                               .downloads = std::string{downloads}};
}

fs::path launcherDir(const fs::path& dataDir) {
    return dataDir / "wine" / "umu-launcher";
}

std::vector<Install> installed(const fs::path& dir) {
    std::vector<Install> found;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{dir, error}) {
        const fs::path run = entry.path() / "umu" / "umu-run";
        std::error_code ec;
        if (!entry.path().filename().string().starts_with('.') && fs::is_regular_file(run, ec)) {
            found.push_back(Install{.version = entry.path().filename().string(), .umuRun = run});
        }
    }
    std::ranges::sort(found, [](const Install& a, const Install& b) {
        return partsOf(a.version) > partsOf(b.version);
    });
    return found;
}

fs::path steamRuntime(const fs::path& folders) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{folders / "umu", error}) {
        std::error_code ec;
        if (entry.path().filename().string().starts_with("steamrt") && entry.is_directory(ec)) {
            return entry.path();
        }
    }
    return {};
}

} // namespace opensu::library::umu
