// Power through a fake runner: the systemctl verbs, refusals, and the refusing backend.
#include "host/power.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

using opensu::host::Power;
using opensu::host::PowerAction;
using opensu::host::Runner;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
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

void verbs() {
    Calls calls;
    const auto power = opensu::host::makeLogindPower(fake(calls, opensu::launch::Captured{}));
    expect(power->perform(PowerAction::Suspend).empty(), "suspend is accepted");
    expect(power->perform(PowerAction::Restart).empty(), "restart is accepted");
    expect(power->perform(PowerAction::ShutDown).empty(), "shut down is accepted");
    expect(calls.programs == std::vector<std::string>(3, "systemctl"), "it runs systemctl");
    expect(calls.arguments ==
               std::vector<std::vector<std::string>>({{"suspend"}, {"reboot"}, {"poweroff"}}),
           "the verbs are suspend, reboot and poweroff");
}

void refusals() {
    Calls calls;
    const auto denied = opensu::host::makeLogindPower(
        fake(calls, opensu::launch::Captured{1, "  Access denied\nmore\n"}));
    expect(denied->perform(PowerAction::ShutDown) == "Access denied",
           "the first output line is the reason");
    const auto silent = opensu::host::makeLogindPower(fake(calls, opensu::launch::Captured{1, ""}));
    expect(silent->perform(PowerAction::Restart) == "systemctl refused to reboot",
           "a silent refusal still has a reason");
    const auto missing = opensu::host::makeLogindPower(fake(calls, std::nullopt));
    expect(missing->perform(PowerAction::Suspend) == "systemctl could not be run",
           "a missing systemctl is said");
}

void refusing() {
    const auto power = opensu::host::makeRefusingPower("power is off in a hidden run");
    expect(power->perform(PowerAction::ShutDown) == "power is off in a hidden run",
           "the refusing backend gives its reason");
}

void labels() {
    expect(std::string{opensu::host::label(PowerAction::Suspend)} == "Sleep" &&
               std::string{opensu::host::label(PowerAction::Restart)} == "Restart" &&
               std::string{opensu::host::label(PowerAction::ShutDown)} == "Shut down" &&
               std::string{opensu::host::label(PowerAction::SwitchToDesktop)} ==
                   "Switch to desktop",
           "the labels");
}

void switchingToTheDesktop() {
    namespace fs = std::filesystem;
    const fs::path bin = fs::path{OPENSU_TEST_SCRATCH} / "power" / "bin";
    fs::remove_all(bin.parent_path());
    fs::create_directories(bin);
    Calls calls;
    const fs::path note = bin.parent_path() / "run" / "desktop";
    const opensu::host::SessionExit exit{
        {bin}, opensu::host::DesktopRequest{note}, "s-compositor.scope"};
    const auto bare = opensu::host::makeLogindPower(fake(calls, opensu::launch::Captured{}), exit);
    expect(bare->perform(PowerAction::SwitchToDesktop).empty(), "ending the session is accepted");
    expect(calls.programs == std::vector<std::string>{"systemctl"} &&
               calls.arguments ==
                   std::vector<std::vector<std::string>>{{"--user", "stop", "s-compositor.scope"}},
           "without steamos-session-select it stops the session's Gamescope scope");
    expect(fs::exists(note), "the login session is told to start the desktop");
    expect(opensu::host::DesktopRequest{note}.consume(), "and finds the note once");

    const fs::path select = bin / "steamos-session-select";
    std::ofstream{select} << "#!/bin/sh\n";
    fs::permissions(select, fs::perms::owner_all);
    calls = {};
    const auto steamos =
        opensu::host::makeLogindPower(fake(calls, opensu::launch::Captured{}), exit);
    expect(steamos->perform(PowerAction::SwitchToDesktop).empty(),
           "the SteamOS switch is accepted");
    expect(calls.programs == std::vector<std::string>{"steamos-session-select"} &&
               calls.arguments == std::vector<std::vector<std::string>>{{"plasma"}},
           "with steamos-session-select it selects plasma");
    const auto denied = opensu::host::makeLogindPower(
        fake(calls, opensu::launch::Captured{1, "no session\n"}), exit);
    expect(denied->perform(PowerAction::SwitchToDesktop) == "no session", "its refusal is said");

    calls = {};
    fs::remove_all(note.parent_path());
    std::ofstream{note.parent_path()} << "a file, not a directory\n";
    const auto unwritable =
        opensu::host::makeLogindPower(fake(calls, opensu::launch::Captured{}), exit);
    expect(!unwritable->perform(PowerAction::SwitchToDesktop).empty() && calls.programs.empty(),
           "when the note cannot be left nothing is ended and the reason is said");
    fs::remove_all(bin.parent_path());
}

} // namespace

int main() {
    verbs();
    refusals();
    refusing();
    labels();
    switchingToTheDesktop();
    return 0;
}
