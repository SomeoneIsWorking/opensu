// Power through a fake runner: the systemctl verbs, refusals, and the refusing backend.
#include "host/power.hpp"

#include <cstdio>
#include <cstdlib>

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
               std::string{opensu::host::label(PowerAction::ShutDown)} == "Shut down",
           "the labels");
}

} // namespace

int main() {
    verbs();
    refusals();
    refusing();
    labels();
    return 0;
}
