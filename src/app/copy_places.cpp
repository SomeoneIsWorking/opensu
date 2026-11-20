#include "copy_places.hpp"

#include "library/game_folder.hpp"
#include "library/gog_installs.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using library::Source;

/// The platform name `InstallCopy::platform` gives this OS's own builds.
std::string_view hostPlatform() noexcept {
    switch (library::folder::thisHost()) {
    case library::folder::Host::Windows:
        return "windows";
    case library::folder::Host::Linux:
        return "linux";
    case library::folder::Host::Mac:
        return "mac";
    }
    return "linux";
}

/// Whether a build for `platform` runs here.
bool runsHere(std::string_view platform) noexcept {
#ifdef __ANDROID__
    return false;
#else
    return platform == hostPlatform() || platform == "windows";
#endif
}

} // namespace

bool CopyPlaces::takes(std::string_view store) const {
#ifdef __ANDROID__
    return false;
#else
    if (store == library::key(Source::Steam)) {
        return !places_.fumaroleApps.empty();
    }
    return store == library::key(Source::Gog) || store == library::key(Source::Epic) ||
           store == library::key(Source::Own);
#endif
}

std::optional<fs::path> CopyPlaces::folderFor(const lan::wire::InstallCopy& install,
                                              std::string& why) const {
    if (!takes(install.store)) {
        why = "this device has nothing that takes it over";
        return std::nullopt;
    }
    if (!runsHere(install.platform)) {
        why = "it is a build for " + install.platform;
        return std::nullopt;
    }
    if (install.store == library::key(Source::Steam)) {
        return places_.fumaroleApps / install.folder;
    }
    if (install.store == library::key(Source::Gog)) {
        return library::gog::Paths::under(places_.dataDir)
                   .gameRoot(places_.storeFolder(Source::Gog), install.storeId) /
               install.folder;
    }
    if (install.store == library::key(Source::Epic)) {
        return places_.storeFolder(Source::Epic).value_or(places_.epicDefault) / install.folder;
    }
    const std::vector<fs::path> folders = places_.gameFolders();
    if (folders.empty()) {
        why = "add a games folder first";
        return std::nullopt;
    }
    return folders.front() / install.folder;
}

} // namespace opensu::app
