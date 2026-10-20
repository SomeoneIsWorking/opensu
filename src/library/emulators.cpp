#include "emulators.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <system_error>

#include "lucent/log.h"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

std::string nameKey(std::string_view name) {
    std::string out;
    for (const unsigned char c : name) {
        if (std::isalnum(c) != 0) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

bool executable(const fs::path& file) {
    std::error_code ec;
    const fs::file_status status = fs::status(file, ec);
    return !ec && fs::is_regular_file(status) &&
           (status.permissions() & fs::perms::owner_exec) != fs::perms::none;
}

/// The version a Flatpak's metainfo names first, or nothing.
std::string flatpakVersion(const fs::path& app, const std::string& appId) {
    const fs::path share = app / "current" / "active" / "files" / "share";
    for (const fs::path& file : {share / "metainfo" / (appId + ".metainfo.xml"),
                                 share / "metainfo" / (appId + ".appdata.xml"),
                                 share / "appdata" / (appId + ".appdata.xml")}) {
        std::ifstream in{file};
        if (!in) {
            continue;
        }
        const std::string body{std::istreambuf_iterator<char>{in},
                               std::istreambuf_iterator<char>{}};
        const std::regex release{R"re(<release[^>]*\sversion="([^"]+)")re"};
        if (std::smatch found; std::regex_search(body, found, release)) {
            return found[1].str();
        }
    }
    return {};
}

std::vector<std::string> commandOf(std::string program, const EmulatorLauncher& launcher) {
    std::vector<std::string> command{std::move(program)};
    command.insert(command.end(), launcher.args.begin(), launcher.args.end());
    return command;
}

std::optional<EmulatorInstall> appImageIn(const EmulatorLauncher& launcher,
                                          const EmulatorSearch& search) {
    for (const fs::path& dir : search.appImageDirs) {
        std::error_code ec;
        std::vector<fs::path> found;
        for (const fs::directory_entry& entry : fs::directory_iterator{dir, ec}) {
            const std::string key = nameKey(entry.path().filename().string());
            if (key.starts_with(launcher.appImage) && key.ends_with("appimage") &&
                executable(entry.path())) {
                found.push_back(entry.path());
            }
        }
        if (!found.empty()) {
            // Sorted, so the same AppImage wins on every run.
            std::ranges::sort(found);
            const fs::path& file = found.front();
            return EmulatorInstall{{},
                                   InstallSource::AppImage,
                                   file.string(),
                                   {},
                                   commandOf(file.string(), launcher),
                                   appImageRefusal(file, search.fuse)};
        }
    }
    return std::nullopt;
}

/// How `launcher` is installed, or nothing when it is not.
std::optional<EmulatorInstall> locate(const EmulatorLauncher& launcher,
                                      const EmulatorSearch& search) {
    for (const std::string& program : launcher.programs) {
        for (const fs::path& dir : search.executablePath) {
            if (executable(dir / program)) {
                const std::string file = (dir / program).string();
                return EmulatorInstall{{}, InstallSource::Program,    file,
                                       {}, commandOf(file, launcher), {}};
            }
        }
    }
    if (!launcher.appImage.empty()) {
        if (std::optional<EmulatorInstall> found = appImageIn(launcher, search)) {
            return found;
        }
    }
    if (!launcher.flatpak.empty()) {
        for (const fs::path& dir : search.flatpakDirs) {
            std::error_code ec;
            if (fs::is_directory(dir / launcher.flatpak, ec)) {
                EmulatorInstall found{{},
                                      InstallSource::Flatpak,
                                      launcher.flatpak,
                                      flatpakVersion(dir / launcher.flatpak, launcher.flatpak),
                                      {"flatpak", "run", launcher.flatpak},
                                      {}};
                found.command.insert(found.command.end(), launcher.args.begin(),
                                     launcher.args.end());
                return found;
            }
        }
    }
    return std::nullopt;
}

/// How built-in `entry` is installed: its core, on openSU's runner.
std::optional<EmulatorInstall> locateCore(const EmulatorEntry& entry,
                                          const EmulatorSearch& search) {
    std::optional<FoundCore> core = search.cores.find(entry.core);
    if (!core) {
        return std::nullopt;
    }
    const std::string file = core->file.string();
    const bool ours = core->downloaded.has_value();
    std::vector<std::string> command{search.runner.string(), "--core", file};
    if (search.hiddenRunner) {
        command.emplace_back("--hidden");
    }
    if (!search.saves.empty()) {
        command.insert(command.end(), {"--saves", (search.saves / "{system}").string()});
    }
    command.emplace_back("{rom}");
    return EmulatorInstall{entry.id,
                           ours ? InstallSource::DownloadedCore : InstallSource::Core,
                           file,
                           ours ? core->downloaded->date : std::string{},
                           std::move(command),
                           executable(search.runner)
                               ? std::string{}
                               : "openSU's libretro runner is missing at " +
                                     search.runner.string() + "; reinstall openSU"};
}

/// How `entry` is installed: openSU's download first, then each launcher in turn.
std::optional<EmulatorInstall> locate(const EmulatorEntry& entry, const EmulatorSearch& search) {
    if (entry.builtIn()) {
        return locateCore(entry, search);
    }
    std::optional<EmulatorInstall> found;
    if (!search.downloads.empty()) {
        if (const std::optional<DownloadedEmulator> downloaded =
                DownloadedEmulators{search.downloads}.find(entry.id)) {
            const std::string file = downloaded->file.string();
            found = EmulatorInstall{{},
                                    InstallSource::Downloaded,
                                    file,
                                    downloaded->version,
                                    commandOf(file, entry.launchers.front()),
                                    appImageRefusal(downloaded->file, search.fuse)};
        }
    }
    for (auto launcher = entry.launchers.begin(); !found && launcher != entry.launchers.end();
         ++launcher) {
        found = locate(*launcher, search);
    }
    if (found) {
        found->id = entry.id;
    }
    return found;
}

/// A command as the launch that starts `rom` of `system`: `{rom}` is the file, `{system}` inside
/// a word is the system's key.
LaunchSpec specOf(const std::vector<std::string>& command, std::string_view system,
                  const fs::path& rom) {
    constexpr std::string_view systemToken{"{system}"};
    LaunchSpec spec;
    spec.program = command.front();
    bool placed = false;
    for (auto word = std::next(command.begin()); word != command.end(); ++word) {
        if (*word == "{rom}") {
            spec.args.push_back(rom.string());
            placed = true;
        } else if (const std::size_t at = word->find(systemToken); at != std::string::npos) {
            std::string filled = *word;
            filled.replace(at, systemToken.size(), system);
            spec.args.push_back(std::move(filled));
        } else {
            spec.args.push_back(*word);
        }
    }
    if (!placed) {
        spec.args.push_back(rom.string());
    }
    return spec;
}

} // namespace

std::string_view label(InstallSource source) {
    switch (source) {
    case InstallSource::Program:
        return "Package";
    case InstallSource::AppImage:
        return "AppImage";
    case InstallSource::Downloaded:
        return "AppImage (openSU)";
    case InstallSource::Flatpak:
        return "Flatpak";
    case InstallSource::DownloadedCore:
        return "libretro buildbot";
    case InstallSource::Core:
        return "Core";
    }
    return "Package";
}

bool downloadedByOpensu(InstallSource source) noexcept {
    return source == InstallSource::Downloaded || source == InstallSource::DownloadedCore;
}

EmulatorSearch EmulatorSearch::standard(const fs::path& home, const fs::path& dataDir,
                                        std::vector<fs::path> executablePath, fs::path runner,
                                        fs::path saves, bool hiddenRunner) {
    FuseSupport fuse = FuseSupport::detect(executablePath);
    return EmulatorSearch{
        .executablePath = std::move(executablePath),
        .appImageDirs = {home / "Applications", home / "AppImages", home / ".local" / "bin"},
        .flatpakDirs = {home / ".local" / "share" / "flatpak" / "app", "/var/lib/flatpak/app"},
        .downloads = DownloadedEmulators::under(dataDir).root(),
        .fuse = std::move(fuse),
        .cores = CoreSearch::standard(home, dataDir),
        .runner = std::move(runner),
        .saves = std::move(saves),
        .hiddenRunner = hiddenRunner,
    };
}

EmulatorSetup EmulatorSetup::standard(const config::Config& config, bool hidden) {
    return EmulatorSetup{EmulatorCatalog::shipped(config.assetsDir, config.home),
                         EmulatorSearch::standard(config.home, config.dataDir,
                                                  config.executablePath, config.retroRunner,
                                                  config.savesDir, hidden),
                         config.emulators};
}

Emulators Emulators::discover(EmulatorCatalog catalog, const EmulatorSearch& search,
                              const Commands& overrides) {
    Emulators found;
    found.catalog_ = std::move(catalog);
    found.overrides_ = overrides;
    for (const EmulatorEntry& entry : found.catalog_.entries()) {
        std::optional<EmulatorInstall> install = locate(entry, search);
        if (!install) {
            continue;
        }
        if (!install->problem.empty()) {
            lucent::warn("roms", "{} at {} cannot start: {}", entry.name, install->where,
                         install->problem);
        }
        for (const std::string& system : entry.systems) {
            if (install->problem.empty() && !found.commands_.contains(system)) {
                lucent::info("roms", "{} runs {} games: {}", entry.name, system,
                             install->command.front());
                found.commands_.emplace(system, install->command);
                found.names_.emplace(system, entry.name);
            }
        }
        found.installs_.emplace(entry.id, std::move(*install));
    }
    for (const auto& [system, command] : overrides) {
        found.commands_.insert_or_assign(system, command);
        found.names_.insert_or_assign(system, std::string{customName});
    }
    return found;
}

const EmulatorInstall* Emulators::installed(std::string_view id) const {
    const auto found = installs_.find(id);
    return found == installs_.end() ? nullptr : &found->second;
}

std::optional<LaunchSpec> Emulators::launch(std::string_view system, const fs::path& rom) const {
    const auto found = commands_.find(system);
    if (found == commands_.end() || found->second.empty()) {
        return std::nullopt;
    }
    return specOf(found->second, system, rom);
}

std::string Emulators::chosenName(std::string_view system) const {
    const auto found = names_.find(system);
    return found == names_.end() ? std::string{} : found->second;
}

std::vector<EmulatorOption> Emulators::options(std::string_view system, const fs::path& rom) const {
    std::vector<EmulatorOption> options;
    if (const auto custom = overrides_.find(system);
        custom != overrides_.end() && !custom->second.empty()) {
        options.push_back(EmulatorOption{
            std::string{customName}, true, specOf(custom->second, system, rom), {}, false, {}});
    }
    for (const EmulatorEntry& entry : catalog_.entries()) {
        if (!entry.serves(system)) {
            continue;
        }
        EmulatorOption option{entry.name, false, {}, entry.id, entry.downloadable(), {}};
        if (const EmulatorInstall* install = installed(entry.id)) {
            option.problem = install->problem;
            option.installed = install->problem.empty();
            if (option.installed) {
                option.launch = specOf(install->command, system, rom);
            }
        }
        options.push_back(std::move(option));
    }
    return options;
}

std::vector<std::string_view> Emulators::candidates(std::string_view system) const {
    std::vector<std::string_view> names;
    for (const EmulatorEntry& entry : catalog_.entries()) {
        if (entry.serves(system)) {
            names.emplace_back(entry.name);
        }
    }
    return names;
}

} // namespace opensu::library::roms
