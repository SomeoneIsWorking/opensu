#include "desktop_night_light.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"
#include "lucent/text.h"

namespace opensu::host {
namespace {

using json = nlohmann::json;
using Clock = std::chrono::system_clock;

constexpr int minKelvin = 1000;
constexpr int neutralKelvin = 6500;
constexpr std::chrono::minutes askAgainAfter{10};

/// One start-to-end change between day and night.
struct Transition {
    Clock::time_point start;
    Clock::time_point end;
    bool morning{false};
};

using Group = std::map<std::string, std::string, std::less<>>;

/// The keys of kwinrc's [NightColor] group, the last value written of each; a key's `[$i]`-style
/// markers are dropped.
Group nightColorGroup(std::string_view kwinrc) {
    Group group;
    bool inGroup = false;
    std::istringstream lines{std::string{kwinrc}};
    std::string raw;
    while (std::getline(lines, raw)) {
        const std::string_view line = lucent::text::trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            inGroup = line == "[NightColor]";
            continue;
        }
        const std::size_t equals = line.find('=');
        if (!inGroup || equals == std::string_view::npos) {
            continue;
        }
        std::string_view name = lucent::text::trim(line.substr(0, equals));
        name = name.substr(0, name.find('['));
        group[std::string{name}] = std::string{lucent::text::trim(line.substr(equals + 1))};
    }
    return group;
}

std::optional<std::string> valueOf(const Group& group, std::string_view key) {
    const auto found = group.find(key);
    return found == group.end() ? std::nullopt : std::optional{found->second};
}

std::optional<int> integerOf(const std::optional<std::string>& text) {
    int value = 0;
    if (!text ||
        std::from_chars(text->data(), text->data() + text->size(), value).ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

/// KConfig's reading of a bool.
bool boolOf(const std::optional<std::string>& text, bool fallback) {
    if (!text) {
        return fallback;
    }
    const std::string lower = lucent::text::ascii_lower(*text);
    if (lower == "true" || lower == "on" || lower == "yes" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "off" || lower == "no" || lower == "0") {
        return false;
    }
    return fallback;
}

Clock::time_point fromMilliseconds(std::int64_t milliseconds) {
    return Clock::time_point{std::chrono::milliseconds{milliseconds}};
}

std::vector<Transition> transitionsOf(std::span<const NightTimeDay> schedule) {
    std::vector<Transition> transitions;
    for (const NightTimeDay& day : schedule) {
        transitions.push_back({day.morningStart, day.morningEnd, true});
        transitions.push_back({day.eveningStart, day.eveningEnd, false});
    }
    std::ranges::sort(transitions, {}, &Transition::start);
    return transitions;
}

std::optional<std::string> readFile(const std::filesystem::path& file) {
    std::ifstream in{file};
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

} // namespace

NightColorSettings parseNightColor(std::string_view kwinrc) {
    const Group group = nightColorGroup(kwinrc);
    NightColorSettings settings;
    settings.active = boolOf(valueOf(group, "Active"), settings.active);
    // KWin 6.7 knows Constant and DarkLight; it reads anything else as DarkLight.
    const std::optional<std::string> mode = valueOf(group, "Mode");
    settings.constant = mode && (*mode == "Constant" || *mode == "0");
    const auto kelvin = [&group](std::string_view key, int fallback) {
        return std::clamp(integerOf(valueOf(group, key)).value_or(fallback), minKelvin,
                          neutralKelvin);
    };
    settings.dayKelvin = kelvin("DayTemperature", settings.dayKelvin);
    settings.nightKelvin = kelvin("NightTemperature", settings.nightKelvin);
    return settings;
}

std::vector<NightTimeDay> parseNightTimeSchedule(std::string_view text) {
    const json parsed = json::parse(text, nullptr, false);
    const json* results = nullptr;
    if (parsed.is_object() && parsed.contains("data") && parsed["data"].is_array() &&
        !parsed["data"].empty()) {
        results = &parsed["data"][0];
    }
    if (results == nullptr || !results->is_object() || !results->contains("Schedule")) {
        return {};
    }
    // Schedule is (sv): the type, "dynamic", and its a(xxxxx) days.
    const json& schedule = (*results)["Schedule"].value("data", json{});
    if (!schedule.is_array() || schedule.size() != 2 || schedule[0] != "dynamic" ||
        !schedule[1].is_object()) {
        return {};
    }
    const json days = schedule[1].value("data", json::array());
    std::vector<NightTimeDay> parsedDays;
    for (const json& day : days) {
        // noon, morning start, morning end, evening start, evening end, in milliseconds.
        if (!day.is_array() || day.size() != 5 || !std::ranges::all_of(day, [](const json& at) {
                return at.is_number_integer();
            })) {
            return {};
        }
        parsedDays.push_back({fromMilliseconds(day[1].get<std::int64_t>()),
                              fromMilliseconds(day[2].get<std::int64_t>()),
                              fromMilliseconds(day[3].get<std::int64_t>()),
                              fromMilliseconds(day[4].get<std::int64_t>())});
    }
    return parsedDays;
}

std::optional<int> nightColorKelvin(const NightColorSettings& settings,
                                    std::span<const NightTimeDay> schedule, Clock::time_point now) {
    if (!settings.active) {
        return neutralKelvin;
    }
    if (settings.constant) {
        return settings.nightKelvin;
    }
    const std::vector<Transition> transitions = transitionsOf(schedule);
    const auto next = std::ranges::upper_bound(transitions, now, {}, &Transition::start);
    if (next == transitions.begin() || next == transitions.end()) {
        return std::nullopt;
    }
    const Transition& previous = *std::prev(next);
    const int from = previous.morning ? settings.nightKelvin : settings.dayKelvin;
    const int to = previous.morning ? settings.dayKelvin : settings.nightKelvin;
    if (now >= previous.end) {
        return to;
    }
    const double progress = std::chrono::duration<double>(now - previous.start) /
                            std::chrono::duration<double>(previous.end - previous.start);
    // KWin's std::lerp, truncated to an int.
    return static_cast<int>(
        std::lerp(static_cast<double>(from), static_cast<double>(to), progress));
}

DesktopNightLight::DesktopNightLight(Runner run, std::filesystem::path kwinrc)
    : run_{std::move(run)}, kwinrc_{std::move(kwinrc)} {
}

NightColorSettings DesktopNightLight::settings() const {
    return parseNightColor(readFile(kwinrc_).value_or(""));
}

int DesktopNightLight::kelvinAt(Clock::time_point now) {
    const NightColorSettings desktop = settings();
    if (const std::optional<int> kelvin = nightColorKelvin(desktop, schedule_, now)) {
        return *kelvin;
    }
    if (!lastAsked_ || now - *lastAsked_ >= askAgainAfter) {
        lastAsked_ = now;
        const std::optional<launch::Captured> out =
            run_("busctl", {"--user", "--json=short", "--timeout=3", "call", "org.kde.NightTime",
                            "/org/kde/NightTime/Manager", "org.kde.NightTime.Manager", "Subscribe",
                            "a{sv}", "0"});
        std::string refusal = refusalOf("busctl", out, "ask knighttimed for its schedule");
        schedule_ = out && out->status == 0 ? parseNightTimeSchedule(out->output)
                                            : std::vector<NightTimeDay>{};
        if (refusal.empty() && schedule_.empty()) {
            refusal = "knighttimed gave no day and night schedule";
        }
        if (refusal.empty()) {
            lucent::info("nightlight", "knighttimed's schedule covers {} days", schedule_.size());
        } else {
            lucent::warn("nightlight", "{}; the day temperature stands until it can be read",
                         refusal);
        }
    }
    return nightColorKelvin(desktop, schedule_, now).value_or(desktop.dayKelvin);
}

} // namespace opensu::host
