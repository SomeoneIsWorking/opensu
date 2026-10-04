#include "roms.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "lucent/log.h"

namespace iideck::library::roms {
namespace {

namespace fs = std::filesystem;

/// The uppercase extension of a filename, including the dot.
std::string extensionOf(std::string_view name)
{
    const std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string ext{name.substr(dot)};
    for (char& c : ext) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return ext;
}

/// The system a ROM belongs to, or "Unknown".
std::string systemFor(const std::map<std::string, std::string, std::less<>>& extensions, std::string_view name)
{
    const auto found = extensions.find(extensionOf(name));
    return found != extensions.end() ? found->second : std::string{"Unknown"};
}

/// The command for a system, with the ROM path appended after the emulator's
/// own arguments. An unconfigured system yields an empty spec, which the shell
/// reports rather than launching nothing silently.
LaunchSpec launchFor(const std::map<std::string, std::vector<std::string>, std::less<>>& emulators,
    std::string_view system, const fs::path& rom)
{
    const auto found = emulators.find(system);
    if (found == emulators.end() || found->second.empty()) {
        return {};
    }
    LaunchSpec spec;
    spec.program = found->second.front();
    spec.args.assign(std::next(found->second.begin()), found->second.end());
    spec.args.push_back(rom.string());
    return spec;
}

} // namespace

Provider::Provider(std::vector<fs::path> roots,
    std::map<std::string, std::string, std::less<>> extensions,
    std::map<std::string, std::vector<std::string>, std::less<>> emulators)
    : roots_{std::move(roots)}
    , extensions_{std::move(extensions)}
    , emulators_{std::move(emulators)}
{
}

std::vector<Game> Provider::list()
{
    if (roots_.empty()) {
        return {};
    }

    std::vector<Game> games;
    std::vector<std::string> problems;

    for (const fs::path& root : roots_) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            problems.push_back(root.string() + ": not a directory");
            continue;
        }
        // Subdirectories are not descended into: an emulator library is
        // conventionally flat, and deep nesting is usually a mistake.
        for (const fs::directory_entry& entry : fs::directory_iterator{root, ec}) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string name = entry.path().filename().string();
            if (extensions_.find(extensionOf(name)) == extensions_.end()) {
                continue;
            }
            const std::string system = systemFor(extensions_, name);

            Game game;
            game.id = "rom:" + entry.path().string();
            game.source = Source::Rom;
            game.sourceId = system;
            game.title = name.substr(0, name.size() - extensionOf(name).size());
            // The file itself is what makes the entry launchable.
            game.installed = true;
            game.processHint = name;
            game.launch = launchFor(emulators_, system, entry.path());
            games.push_back(std::move(game));
        }
        if (ec) {
            problems.push_back(root.string() + ": " + ec.message());
        }
    }

    if (games.empty() && !problems.empty()) {
        lucent::warn("roms", "no ROMs found in {} configured roots", roots_.size());
    }
    for (const std::string& problem : problems) {
        lucent::warn("roms", "{}", problem);
    }
    return games;
}

std::vector<std::string> Provider::systems() const
{
    std::vector<std::string> out;
    for (const auto& [extension, system] : extensions_) {
        if (std::ranges::find(out, system) == out.end()) {
            out.push_back(system);
        }
    }
    std::ranges::sort(out);
    return out;
}

} // namespace iideck::library::roms