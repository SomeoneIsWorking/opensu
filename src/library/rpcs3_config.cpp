#include "rpcs3_config.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <system_error>

#include "ini_file.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view handlerKey{"  Handler: "};
constexpr std::string_view deviceKey{"  Device: "};
constexpr std::string_view configKey{"  Config:"};
constexpr std::string_view sdlHandler{"SDL"};
constexpr std::string_view nullHandler{"Null"};
constexpr std::string_view mappingIndent{"    "};

struct DefaultBinding {
    std::string_view key;
    std::string_view button;
};

// `sdl_pad_handler::init_config`; the PS button on Guide only.
constexpr std::array defaults{
    DefaultBinding{"Left Stick Left", "LS X-"},
    DefaultBinding{"Left Stick Down", "LS Y-"},
    DefaultBinding{"Left Stick Right", "LS X+"},
    DefaultBinding{"Left Stick Up", "LS Y+"},
    DefaultBinding{"Right Stick Left", "RS X-"},
    DefaultBinding{"Right Stick Down", "RS Y-"},
    DefaultBinding{"Right Stick Right", "RS X+"},
    DefaultBinding{"Right Stick Up", "RS Y+"},
    DefaultBinding{"Start", "Start"},
    DefaultBinding{"Select", "Back"},
    DefaultBinding{"PS Button", "Guide"},
    DefaultBinding{"Square", "West"},
    DefaultBinding{"Cross", "South"},
    DefaultBinding{"Circle", "East"},
    DefaultBinding{"Triangle", "North"},
    DefaultBinding{"Left", "Left"},
    DefaultBinding{"Down", "Down"},
    DefaultBinding{"Right", "Right"},
    DefaultBinding{"Up", "Up"},
    DefaultBinding{"R1", "RB"},
    DefaultBinding{"R2", "RT"},
    DefaultBinding{"R3", "RS"},
    DefaultBinding{"L1", "LB"},
    DefaultBinding{"L2", "LT"},
    DefaultBinding{"L3", "LS"},
};

std::vector<std::string> linesOf(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end;
        lines.emplace_back(text.substr(at, end - at));
        at = end + 1;
    }
    return lines;
}

std::string joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) {
        out += line;
        out += '\n';
    }
    return out;
}

std::string unquoted(std::string_view value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return std::string{value};
}

/// One player's lines, `[begin, end)`, the first its `Player N Input:` header.
struct Block {
    std::size_t begin;
    std::size_t end;
};

std::optional<Block> blockOf(const std::vector<std::string>& lines, int player) {
    const std::string header = "Player " + std::to_string(player + 1) + " Input:";
    for (std::size_t at = 0; at < lines.size(); ++at) {
        if (lines[at] != header) {
            continue;
        }
        std::size_t end = at + 1;
        while (end < lines.size() && (lines[end].empty() || lines[end].front() == ' ')) {
            ++end;
        }
        return Block{.begin = at, .end = end};
    }
    return std::nullopt;
}

/// The line of `prefix` in `block`, or its end.
std::size_t lineOf(const std::vector<std::string>& lines, const Block& block,
                   std::string_view prefix) {
    for (std::size_t at = block.begin + 1; at < block.end; ++at) {
        if (lines[at].starts_with(prefix)) {
            return at;
        }
    }
    return block.end;
}

std::string valueAt(const std::vector<std::string>& lines, std::size_t at,
                    std::string_view prefix) {
    return at < lines.size() && lines[at].starts_with(prefix)
               ? unquoted(std::string_view{lines[at]}.substr(prefix.size()))
               : std::string{};
}

/// `key: value` set in `block`'s top level (`prefix` "  key: ") or its mappings.
void setLine(std::vector<std::string>& lines, Block& block, std::string_view prefix,
             std::string_view value, std::size_t insertAt) {
    const std::size_t at = lineOf(lines, block, prefix);
    std::string line = std::string{prefix} + std::string{value};
    if (at < block.end) {
        lines[at] = std::move(line);
        return;
    }
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAt), std::move(line));
    ++block.end;
}

void bindDefaults(std::vector<std::string>& lines, Block& block) {
    setLine(lines, block, handlerKey, sdlHandler, block.begin + 1);
    if (lineOf(lines, block, configKey) == block.end) {
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(block.end),
                     std::string{configKey});
        ++block.end;
    }
    for (const DefaultBinding& binding : defaults) {
        const std::string prefix = std::string{mappingIndent} + std::string{binding.key} + ": ";
        setLine(lines, block, prefix, binding.button, lineOf(lines, block, configKey) + 1);
    }
}

void bindPlayer(std::vector<std::string>& lines, int player) {
    std::optional<Block> block = blockOf(lines, player);
    if (!block) {
        if (player != 0) {
            return;
        }
        lines.insert(lines.begin(), "Player 1 Input:");
        block = Block{.begin = 0, .end = 1};
    }
    const std::string handler = valueAt(lines, lineOf(lines, *block, handlerKey), handlerKey);
    const std::string device = std::string{virtualPadSdlName} + " " + std::to_string(player + 1);
    if (handler == sdlHandler) {
        lucent::info("rpcs3", "player {} bound to {}", player + 1, device);
    } else if (player == 0 || (!handler.empty() && handler != nullHandler)) {
        bindDefaults(lines, *block);
        lucent::info("rpcs3", "player {} given the standard layout on {}", player + 1, device);
    } else {
        return;
    }
    setLine(lines, *block, deviceKey, device, lineOf(lines, *block, handlerKey) + 1);
}

/// The settings files the rule reads under `setup.config`.
std::vector<fs::path> settingsFiles(const LaunchSetup& setup) {
    const fs::path fallback = setup.config / rpcs3DefaultProfile;
    std::vector<fs::path> files{setup.config / rpcs3GuiSettings, fallback};
    std::error_code error;
    fs::directory_iterator folder(setup.config / rpcs3InputConfigs, error);
    for (; !error && folder != fs::directory_iterator{}; folder.increment(error)) {
        std::error_code inner;
        fs::directory_iterator entry(folder->path(), inner);
        for (; !inner && entry != fs::directory_iterator{}; entry.increment(inner)) {
            if (entry->path().extension() == ".yml" && entry->path() != fallback) {
                files.push_back(entry->path());
            }
        }
    }
    return files;
}

} // namespace

std::string rpcs3PadProfile(std::string_view yml) {
    std::vector<std::string> lines = linesOf(yml);
    for (int player = 0; player < rpcs3Players; ++player) {
        bindPlayer(lines, player);
    }
    const std::string out = joined(lines);
    return out == std::string{yml} + (yml.empty() || yml.ends_with('\n') ? "" : "\n")
               ? std::string{yml}
               : out;
}

std::string rpcs3GuiLaunchSettings(std::string_view ini) {
    return withIniValue(ini, {"main_window", "infoBoxEnabledWelcome"}, "false");
}

std::vector<SettingsWrite> rpcs3Writes(const LaunchSetup& setup) {
    std::vector<SettingsWrite> files;
    const fs::path gui = setup.config / rpcs3GuiSettings;
    for (const fs::path& file : settingsFiles(setup)) {
        if (file != gui && setup.padGuid.empty()) {
            continue;
        }
        std::optional<SettingsWrite> run =
            file == gui ? settingsWriteOf(file,
                                          [](std::string_view ini) {
                                              return rpcs3GuiLaunchSettings(ini);
                                          })
                        : settingsWriteOf(file, [](std::string_view yml) {
                              return rpcs3PadProfile(yml);
                          });
        if (run) {
            files.push_back(std::move(*run));
        }
    }
    return files;
}

} // namespace opensu::library
