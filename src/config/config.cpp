#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <ranges>
#include <string_view>
#include <vector>

#include <langinfo.h>
#include <locale.h>
#include <unistd.h>

#include "lucent/log.h"

namespace iideck::config {
namespace {

std::string_view env(const char* name) {
    const char* raw = std::getenv(name);
    return raw != nullptr ? std::string_view{raw} : std::string_view{};
}

int envInt(const char* name, int fallback) {
    const std::string_view raw = env(name);
    if (raw.empty()) {
        return fallback;
    }
    int value = 0;
    const auto [end, error] = std::from_chars(raw.data(), raw.data() + raw.size(), value);
    if (error != std::errc{} || end != raw.data() + raw.size()) {
        lucent::warn("config", "{} is not a number; using {}", name, fallback);
        return fallback;
    }
    return value;
}

std::uint16_t envPort(const char* name, std::uint16_t fallback) {
    const int value = envInt(name, fallback);
    if (value < 0 || value > 65535) {
        lucent::warn("config", "{} is out of range; using {}", name, fallback);
        return fallback;
    }
    return static_cast<std::uint16_t>(value);
}

bool envBool(const char* name, bool fallback) {
    const std::string_view raw = env(name);
    if (raw.empty()) {
        return fallback;
    }
    return raw == "1" || raw == "true" || raw == "yes";
}

HomeMode envHomeMode(const char* name, HomeMode fallback) {
    const std::string_view raw = env(name);
    if (raw.empty()) {
        return fallback;
    }
    if (raw == "standard") {
        return HomeMode::Standard;
    }
    if (raw == "wiisu") {
        return HomeMode::WiiSu;
    }
    lucent::warn("config", "{} is not standard or wiisu; using standard", name);
    return fallback;
}

/// Splits a colon-separated path list. A trailing or repeated separator is not
/// an error, so an empty element is simply dropped.
std::vector<std::filesystem::path> splitPaths(std::string_view raw) {
    std::vector<std::filesystem::path> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t end = raw.find(':', start);
        const std::size_t stop = end == std::string_view::npos ? raw.size() : end;
        if (stop > start) {
            out.emplace_back(raw.substr(start, stop - start));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

/// Splits a shell-like command into words, honouring double quotes so an
/// argument can contain spaces.
std::vector<std::string> splitWords(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    bool inQuotes = false;
    for (const char c : text) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && (c == ' ' || c == '\t')) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

/// Reads "SYSTEM=program arg;SYSTEM2=program". Splitting on '=' and then on
/// whitespace keeps a command's own arguments unambiguous. A system name is
/// upper-cased, because a ROM's system is matched that way.
EmulatorCommands parseEmulators(std::string_view raw) {
    EmulatorCommands out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t end = std::min(raw.find(';', start), raw.size());
        const std::string_view entry = raw.substr(start, end - start);
        if (!entry.empty()) {
            const std::size_t equals = entry.find('=');
            if (equals != std::string_view::npos) {
                std::string name{entry.substr(0, equals)};
                std::ranges::transform(name, name.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                std::vector<std::string> words = splitWords(entry.substr(equals + 1));
                if (name.empty() || words.empty()) {
                    lucent::warn("config", "ignoring emulator entry without a system or program");
                } else {
                    out.emplace(std::move(name), std::move(words));
                }
            } else {
                lucent::warn("config", "ignoring emulator entry without '=': {}", entry);
            }
        }
        if (end == raw.size()) {
            break;
        }
        start = end + 1;
    }
    return out;
}

/// The LC_TIME locale's clock convention; the C locale's "%H:%M:%S" when it cannot be loaded.
bool localeClock24Hour() {
    const locale_t locale = newlocale(LC_TIME_MASK, "", static_cast<locale_t>(nullptr));
    if (locale == static_cast<locale_t>(nullptr)) {
        lucent::warn("config", "the time locale could not be loaded; using a 24-hour clock");
        return true;
    }
    const bool twentyFour = timeFormatIs24Hour(nl_langinfo_l(T_FMT, locale));
    freelocale(locale);
    return twentyFour;
}

} // namespace

bool timeFormatIs24Hour(std::string_view format) noexcept {
    for (std::size_t at = format.find('%'); at != std::string_view::npos && at + 1 < format.size();
         at = format.find('%', at + 2)) {
        const char directive = format[at + 1];
        if (directive == 'I' || directive == 'l' || directive == 'r') {
            return false;
        }
    }
    return true;
}

std::filesystem::path gamescopeBeside(const std::filesystem::path& executable) {
    if (executable.empty()) {
        return {};
    }
    return executable.parent_path().parent_path() / IIDECK_GAMESCOPE_RELATIVE;
}

const Config& read() {
    // Deliberately function-local: the environment is read once and never again,
    // so every holder of this reference sees the same immutable value.
    static const Config config = [] {
        Config value;
        if (const std::string_view home = env("HOME"); !home.empty()) {
            value.home = std::filesystem::path{home};
        }
        value.steamRoots = splitPaths(env("IIDECK_STEAM_ROOTS"));
        value.romRoots = splitPaths(env("IIDECK_ROM_ROOTS"));
        value.emulators = parseEmulators(env("IIDECK_EMULATORS"));
        std::error_code error;
        const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", error);
        if (const std::string_view assets = env("IIDECK_ASSETS"); !assets.empty()) {
            value.assetsDir = std::filesystem::path{assets};
        } else {
            value.assetsDir = self.parent_path().parent_path() / "share" / "iideck";
        }
        value.gamescope = gamescopeBeside(self);
        if (const std::string_view cache = env("XDG_CACHE_HOME"); !cache.empty()) {
            value.cacheDir = std::filesystem::path{cache} / "iideck";
        } else {
            value.cacheDir = value.home / ".cache" / "iideck";
        }
        if (const std::string_view data = env("XDG_DATA_HOME"); !data.empty()) {
            value.dataDir = std::filesystem::path{data} / "iideck";
        } else {
            value.dataDir = value.home / ".local" / "share" / "iideck";
        }
        value.homeMode = envHomeMode("IIDECK_HOME_MODE", value.homeMode);
        value.clock24Hour = localeClock24Hour();
        value.width = envInt("IIDECK_WIDTH", value.width);
        value.height = envInt("IIDECK_HEIGHT", value.height);
        value.controlPort = envPort("IIDECK_CONTROL_PORT", value.controlPort);
        value.controlChannel = envBool("IIDECK_CONTROL_CHANNEL", value.controlChannel);
        value.insideGamescope = !env("GAMESCOPE_WAYLAND_DISPLAY").empty();
        value.session = std::string{env("IIDECK_SESSION")};
        value.sessionInherited = !value.session.empty();
        if (value.session.empty()) {
            value.session = "iideck-" + std::to_string(getpid());
        }
        value.executablePath = splitPaths(env("PATH"));
        return value;
    }();
    return config;
}

} // namespace iideck::config