// Backlight against a fake /sys/class/backlight and a fake runner.
#include "host/backlight.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

namespace fs = std::filesystem;
using opensu::host::Runner;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

fs::path freshRoot(const char* name) {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "backlight" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void device(const fs::path& root, const char* name, const char* brightness, const char* max) {
    fs::create_directories(root / name);
    std::ofstream{root / name / "brightness"} << brightness << '\n';
    std::ofstream{root / name / "max_brightness"} << max << '\n';
}

struct Calls {
    std::vector<std::string> programs;
    std::vector<std::vector<std::string>> arguments;
};

Runner fake(Calls& calls, const std::optional<opensu::launch::Captured>& answer) {
    return [&calls, answer](const std::string& program, const std::vector<std::string>& args) {
        calls.programs.push_back(program);
        calls.arguments.push_back(args);
        return answer;
    };
}

void reading() {
    Calls calls;
    const fs::path root = freshRoot("reading");
    expect(opensu::host::makeLogindBacklight(fake(calls, {}), root) == nullptr,
           "no device, no backlight");
    expect(opensu::host::makeLogindBacklight(fake(calls, {}), root / "absent") == nullptr,
           "a missing class directory, no backlight");
    fs::create_directories(root / "broken");
    std::ofstream{root / "broken" / "brightness"} << "text\n";
    std::ofstream{root / "broken" / "max_brightness"} << "100\n";
    expect(opensu::host::makeLogindBacklight(fake(calls, {}), root) == nullptr,
           "an unreadable device is skipped");
    device(root, "b_second", "10", "10");
    device(root, "a_first", "1", "3");
    const auto light = opensu::host::makeLogindBacklight(fake(calls, {}), root);
    expect(light != nullptr, "a device is found");
    expect(light->percent() == 33, "the first device by name, 1/3 rounds to 33");
    std::ofstream{root / "a_first" / "brightness"} << "2\n";
    expect(light->percent() == 67, "it reads the file each time, 2/3 rounds to 67");
    expect(calls.programs.empty(), "reading runs nothing");
}

void writing() {
    Calls calls;
    const fs::path root = freshRoot("writing");
    device(root, "intel_backlight", "400", "1000");
    const auto light =
        opensu::host::makeLogindBacklight(fake(calls, opensu::launch::Captured{}), root);
    expect(light->setPercent(50).empty(), "a set is accepted");
    expect(calls.programs.back() == "busctl", "it runs busctl");
    expect(calls.arguments.back() ==
               std::vector<std::string>({"--system", "call", "org.freedesktop.login1",
                                         "/org/freedesktop/login1/session/auto",
                                         "org.freedesktop.login1.Session", "SetBrightness", "ssu",
                                         "backlight", "intel_backlight", "500"}),
           "the exact SetBrightness call");
    expect(light->setPercent(0).empty() && calls.arguments.back().back() == "10",
           "0 is raised to 1 percent");
    expect(light->setPercent(250).empty() && calls.arguments.back().back() == "1000",
           "above 100 is 100 percent");
    device(root, "tiny", "1", "5");
    fs::remove_all(root / "intel_backlight");
    const auto small =
        opensu::host::makeLogindBacklight(fake(calls, opensu::launch::Captured{}), root);
    expect(small->setPercent(1).empty() && calls.arguments.back().back() == "1",
           "the raw value is never below 1");
}

void refused() {
    Calls calls;
    const fs::path root = freshRoot("refused");
    device(root, "lcd", "5", "10");
    const auto denied = opensu::host::makeLogindBacklight(
        fake(calls, opensu::launch::Captured{1, " Access denied\nx\n"}), root);
    expect(denied->setPercent(40) == "Access denied", "the reason is the first line");
    const auto missing = opensu::host::makeLogindBacklight(fake(calls, std::nullopt), root);
    expect(missing->setPercent(40) == "busctl could not be run", "a missing busctl is said");
}

void readOnly() {
    Calls calls;
    const fs::path root = freshRoot("readonly");
    device(root, "lcd", "50", "100");
    const auto light = opensu::host::makeReadOnlyBacklight(
        opensu::host::makeLogindBacklight(fake(calls, opensu::launch::Captured{}), root));
    expect(light->percent() == 50, "it reads the real value once");
    std::ofstream{root / "lcd" / "brightness"} << "80\n";
    expect(light->percent() == 50, "then holds it");
    expect(light->setPercent(0).empty() && light->percent() == 1, "a set is held, clamped to 1");
    expect(light->setPercent(70).empty() && light->percent() == 70, "and held again");
    expect(calls.programs.empty(), "nothing is ever run");
}

} // namespace

int main() {
    reading();
    writing();
    refused();
    readOnly();
    return 0;
}
