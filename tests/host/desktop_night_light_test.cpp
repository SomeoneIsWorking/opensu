// The desktop's night light: kwinrc's [NightColor] group, knighttimed's schedule as busctl prints
// it, KWin's temperature through the day, and when knighttimed is asked.
#include "host/desktop_night_light.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;
using opensu::host::nightColorKelvin;
using opensu::host::NightColorSettings;
using opensu::host::NightTimeDay;
using opensu::host::parseNightColor;
using opensu::host::parseNightTimeSchedule;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// The first day's start.
constexpr Clock::time_point dayOne{std::chrono::milliseconds{1792620000000}};

long long millisecondsAt(std::chrono::minutes since) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               (dayOne + since).time_since_epoch())
        .count();
}

/// Mornings from 06:00 to 06:30 and evenings from 18:00 to 18:30, for `days` days.
std::string schedule(int days) {
    std::string out =
        R"({"type":"a{sv}","data":[{"Cookie":{"type":"u","data":1},)"
        R"j("Schedule":{"type":"(sv)","data":["dynamic",{"type":"a(xxxxx)","data":[)j";
    for (int day = 0; day < days; ++day) {
        const std::chrono::minutes start = std::chrono::hours{24} * day;
        out += (day == 0 ? "[" : ",[") + std::to_string(millisecondsAt(start + 12h)) + "," +
               std::to_string(millisecondsAt(start + 6h)) + "," +
               std::to_string(millisecondsAt(start + 6h + 30min)) + "," +
               std::to_string(millisecondsAt(start + 18h)) + "," +
               std::to_string(millisecondsAt(start + 18h + 30min)) + "]";
    }
    return out + "]}]}}]}";
}

void settings() {
    expect(parseNightColor("") == NightColorSettings{}, "no group: KWin's defaults, off");
    const NightColorSettings onlyActive = parseNightColor("[Plugins]\nActive=false\n\n"
                                                          "[NightColor]\nActive=true\n");
    expect(onlyActive.active && !onlyActive.constant && onlyActive.dayKelvin == 6500 &&
               onlyActive.nightKelvin == 4500,
           "Active alone: day and night by the schedule at 6500 K and 4500 K");
    const NightColorSettings set =
        parseNightColor("[NightColor]\nActive[$i]=true\nMode=Constant\nDayTemperature=5000\n"
                        "NightTemperature=300\n[Other]\nNightTemperature=2000\n");
    expect(set.active && set.constant && set.dayKelvin == 5000 && set.nightKelvin == 1000,
           "markers are dropped, Constant is read and a temperature is clamped to 1000 K");
    expect(!parseNightColor("[NightColor]\nActive=true\nMode=Automatic\n").constant &&
               !parseNightColor("[NightColor]\nMode=DarkLight\n").constant &&
               parseNightColor("[NightColor]\nMode=0\n").constant,
           "any mode but Constant follows the schedule, as KWin reads it");
    expect(parseNightColor("[NightColor]\nDayTemperature=9000\n").dayKelvin == 6500,
           "a day temperature above neutral is neutral");
}

void scheduleParsing() {
    const std::vector<NightTimeDay> days = parseNightTimeSchedule(schedule(2));
    expect(days.size() == 2 && days[0].morningStart == dayOne + 6h &&
               days[0].eveningEnd == dayOne + 18h + 30min &&
               days[1].morningEnd == dayOne + 24h + 6h + 30min,
           "each day's four times, in milliseconds");
    expect(parseNightTimeSchedule("").empty() && parseNightTimeSchedule(R"({"data":[{}]})").empty(),
           "no output, no schedule");
    expect(parseNightTimeSchedule(
               R"({"data":[{"Schedule":{"data":["static",{"data":[[1,2,3,4,5]]}]}}]})")
               .empty(),
           "a schedule type other than dynamic is not read");
}

void temperatures() {
    const std::vector<NightTimeDay> days = parseNightTimeSchedule(schedule(2));
    const NightColorSettings on{.active = true};
    expect(nightColorKelvin(NightColorSettings{}, days, dayOne + 22h) == 6500,
           "off is neutral at night");
    expect(nightColorKelvin({.active = true, .constant = true}, days, dayOne + 12h) == 4500,
           "constant is the night temperature at noon");
    expect(nightColorKelvin(on, days, dayOne + 12h) == 6500, "noon is day");
    expect(nightColorKelvin(on, days, dayOne + 18h + 15min) == 5500,
           "half way through the evening is half way to night");
    expect(nightColorKelvin(on, days, dayOne + 22h) == 4500, "the evening's end is night");
    expect(nightColorKelvin(on, days, dayOne + 24h + 6h + 15min) == 5500,
           "half way through the morning is half way back to day");
    expect(nightColorKelvin({.active = true, .dayKelvin = 6000, .nightKelvin = 3000}, days,
                            dayOne + 18h + 7min + 30s) == 5250,
           "the player's temperatures, a quarter through the evening");
    expect(!nightColorKelvin(on, days, dayOne + 5h) && !nightColorKelvin(on, days, dayOne + 47h),
           "a moment the schedule does not reach around has no temperature");
}

struct Calls {
    int count{0};
    std::vector<std::string> arguments;
};

opensu::host::Runner knighttimed(Calls& calls, const std::optional<std::string>& output) {
    return [&calls, output](
               const std::string& program,
               const std::vector<std::string>& args) -> std::optional<opensu::launch::Captured> {
        ++calls.count;
        calls.arguments = args;
        calls.arguments.insert(calls.arguments.begin(), program);
        if (!output) {
            return opensu::launch::Captured{1, "Call failed: The name is not activatable\n"};
        }
        return opensu::launch::Captured{0, *output};
    };
}

void reader() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "desktop_night_light";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path kwinrc = root / "kwinrc";
    std::ofstream{kwinrc} << "[NightColor]\nActive=true\n";

    Calls calls;
    opensu::host::DesktopNightLight desktop{knighttimed(calls, schedule(2)), kwinrc};
    expect(desktop.kelvinAt(dayOne + 22h) == 4500 && calls.count == 1,
           "the schedule is asked for once and gives night");
    expect(calls.arguments.size() > 8 && calls.arguments[0] == "busctl" &&
               calls.arguments[1] == "--user" && calls.arguments[5] == "org.kde.NightTime",
           "knighttimed on the session bus");
    expect(desktop.kelvinAt(dayOne + 24h + 12h) == 6500 && calls.count == 1,
           "a moment the held schedule covers asks nothing");
    std::ofstream{kwinrc} << "[NightColor]\nActive=false\n";
    expect(desktop.kelvinAt(dayOne + 22h) == 6500, "kwinrc is read again each time");

    Calls failing;
    std::ofstream{kwinrc} << "[NightColor]\nActive=true\nDayTemperature=6000\n";
    opensu::host::DesktopNightLight absent{knighttimed(failing, std::nullopt), kwinrc};
    expect(absent.kelvinAt(dayOne + 22h) == 6000 && failing.count == 1,
           "without knighttimed the day temperature stands");
    expect(absent.kelvinAt(dayOne + 22h + 5min) == 6000 && failing.count == 1,
           "knighttimed is not asked again within ten minutes");
    expect(absent.kelvinAt(dayOne + 22h + 10min) == 6000 && failing.count == 2,
           "and is asked again after ten");
    fs::remove_all(root);
}

} // namespace

int main() {
    settings();
    scheduleParsing();
    temperatures();
    reader();
    std::printf("desktop_night_light: all checks passed\n");
    return 0;
}
