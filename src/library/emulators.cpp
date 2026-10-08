#include "emulators.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "lucent/log.h"

namespace iideck::library::roms {
namespace {

namespace fs = std::filesystem;

struct EmulatorRule {
    std::string_view name;
    std::vector<std::string_view> systems;
    /// Program names on PATH.
    std::vector<std::string_view> programs;
    /// AppImage file names start with this, compared lower-case with only letters and digits.
    std::string_view appImage;
    std::string_view flatpak;
    std::vector<std::string_view> args;
};

const std::vector<EmulatorRule> rules{
    {"Dolphin",
     {"gc", "wii"},
     {"dolphin-emu"},
     "dolphin",
     "org.DolphinEmu.dolphin-emu",
     {"-b", "-e", "{rom}"}},
    {"Cemu", {"wiiu"}, {"cemu", "Cemu"}, "cemu", "info.cemu.Cemu", {"-f", "-g", "{rom}"}},
    {"Eden", {"switch"}, {"eden"}, "eden", "dev.eden_emu.eden", {"-f", "-g", "{rom}"}},
    {"Ryujinx",
     {"switch"},
     {"ryujinx", "Ryujinx"},
     "ryujinx",
     "io.github.ryubing.Ryujinx",
     {"--fullscreen", "{rom}"}},
    {"PCSX2",
     {"ps2"},
     {"pcsx2-qt"},
     "pcsx2",
     "net.pcsx2.PCSX2",
     {"-batch", "-nogui", "-fullscreen", "--", "{rom}"}},
    {"RPCS3",
     {"ps3"},
     {"rpcs3"},
     "rpcs3",
     "net.rpcs3.RPCS3",
     {"--no-gui", "--fullscreen", "{rom}"}},
    // The launcher starts the core version chosen in its own settings.
    {"shadPS4",
     {"ps4"},
     {"shadps4-qt-launcher"},
     "shadps4qtlauncher",
     "",
     {"-e", "default", "-g", "{rom}"}},
    {"shadPS4", {"ps4"}, {"shadps4"}, "shadps4", "net.shadps4.shadPS4", {"-g", "{rom}"}},
    {"DuckStation",
     {"psx"},
     {"duckstation-qt"},
     "duckstation",
     "org.duckstation.DuckStation",
     {"-batch", "-fullscreen", "--", "{rom}"}},
    {"PPSSPP",
     {"psp"},
     {"PPSSPPSDL", "PPSSPPQt", "ppsspp"},
     "ppsspp",
     "org.ppsspp.PPSSPP",
     {"--fullscreen", "{rom}"}},
    {"melonDS", {"nds"}, {"melonDS"}, "melonds", "net.kuribo64.melonDS", {"-f", "{rom}"}},
    {"Azahar", {"n3ds"}, {"azahar"}, "azahar", "org.azahar_emu.Azahar", {"{rom}"}},
    {"mGBA", {"gb", "gbc", "gba"}, {"mgba-qt", "mgba"}, "mgba", "io.mgba.mGBA", {"-f", "{rom}"}},
    {"xemu", {"xbox"}, {"xemu"}, "xemu", "app.xemu.xemu", {"-dvd_path", "{rom}"}},
    {"Xenia Canary", {"xbox360"}, {"xenia_canary"}, "xenia", "", {"--fullscreen=true", "{rom}"}},
};

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

/// The program and leading words that run `rule`, or nothing when it is not installed.
std::optional<std::vector<std::string>> locate(const EmulatorRule& rule,
                                               const EmulatorSearch& search) {
    for (const std::string_view program : rule.programs) {
        for (const fs::path& dir : search.executablePath) {
            if (executable(dir / program)) {
                return std::vector<std::string>{(dir / program).string()};
            }
        }
    }
    for (const fs::path& dir : search.appImageDirs) {
        std::error_code ec;
        std::vector<fs::path> found;
        for (const fs::directory_entry& entry : fs::directory_iterator{dir, ec}) {
            const std::string file = entry.path().filename().string();
            const std::string key = nameKey(file);
            if (key.starts_with(rule.appImage) && key.ends_with("appimage") &&
                executable(entry.path())) {
                found.push_back(entry.path());
            }
        }
        if (!found.empty()) {
            // Sorted, so the same AppImage wins on every run.
            std::ranges::sort(found);
            return std::vector<std::string>{found.front().string()};
        }
    }
    if (!rule.flatpak.empty()) {
        for (const fs::path& dir : search.flatpakDirs) {
            std::error_code ec;
            if (fs::is_directory(dir / rule.flatpak, ec)) {
                return std::vector<std::string>{"flatpak", "run", std::string{rule.flatpak}};
            }
        }
    }
    return std::nullopt;
}

} // namespace

EmulatorSearch EmulatorSearch::standard(const fs::path& home,
                                        std::vector<fs::path> executablePath) {
    return EmulatorSearch{
        .executablePath = std::move(executablePath),
        .appImageDirs = {home / "Applications", home / "AppImages", home / ".local" / "bin"},
        .flatpakDirs = {home / ".local" / "share" / "flatpak" / "app", "/var/lib/flatpak/app"},
    };
}

Emulators Emulators::discover(const EmulatorSearch& search, const Commands& overrides) {
    Emulators found;
    for (const EmulatorRule& rule : rules) {
        // An emulator already chosen for every system of this rule needs no search.
        const bool needed = std::ranges::any_of(rule.systems, [&found](std::string_view system) {
            return !found.commands_.contains(system);
        });
        if (!needed) {
            continue;
        }
        std::optional<std::vector<std::string>> command = locate(rule, search);
        if (!command) {
            continue;
        }
        command->insert(command->end(), rule.args.begin(), rule.args.end());
        for (const std::string_view system : rule.systems) {
            if (!found.commands_.contains(system)) {
                lucent::info("roms", "{} runs {} games: {}", rule.name, system, command->front());
                found.commands_.emplace(std::string{system}, *command);
            }
        }
    }
    for (const auto& [system, command] : overrides) {
        found.commands_.insert_or_assign(system, command);
    }
    return found;
}

std::optional<LaunchSpec> Emulators::launch(std::string_view system, const fs::path& rom) const {
    const auto found = commands_.find(system);
    if (found == commands_.end() || found->second.empty()) {
        return std::nullopt;
    }
    LaunchSpec spec;
    spec.program = found->second.front();
    bool placed = false;
    for (auto word = std::next(found->second.begin()); word != found->second.end(); ++word) {
        if (*word == "{rom}") {
            spec.args.push_back(rom.string());
            placed = true;
        } else {
            spec.args.push_back(*word);
        }
    }
    if (!placed) {
        spec.args.push_back(rom.string());
    }
    return spec;
}

std::vector<std::string_view> Emulators::candidates(std::string_view system) {
    std::vector<std::string_view> names;
    for (const EmulatorRule& rule : rules) {
        if (std::ranges::find(rule.systems, system) != rule.systems.end() &&
            std::ranges::find(names, rule.name) == names.end()) {
            names.push_back(rule.name);
        }
    }
    return names;
}

} // namespace iideck::library::roms
