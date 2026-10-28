#include "game_end.hpp"

#include <array>
#include <csignal>
#include <string>
#include <string_view>

namespace opensu::app {
namespace {

struct SignalName {
    int number;
    std::string_view name;
};

constexpr std::array<SignalName, 13> signalNames{{
    {SIGSEGV, "SIGSEGV"},
    {SIGABRT, "SIGABRT"},
    {SIGBUS, "SIGBUS"},
    {SIGFPE, "SIGFPE"},
    {SIGILL, "SIGILL"},
    {SIGTRAP, "SIGTRAP"},
    {SIGSYS, "SIGSYS"},
    {SIGQUIT, "SIGQUIT"},
    {SIGPIPE, "SIGPIPE"},
    {SIGALRM, "SIGALRM"},
    {SIGUSR1, "SIGUSR1"},
    {SIGUSR2, "SIGUSR2"},
    {SIGXCPU, "SIGXCPU"},
}};

/// The highest real-time signal Linux numbers.
constexpr int lastSignal = 64;

} // namespace

std::string crashName(int exitStatus) {
    const int signal = exitStatus - 128;
    if (signal < 1 || signal > lastSignal || signal == SIGTERM || signal == SIGKILL ||
        signal == SIGINT || signal == SIGHUP) {
        return {};
    }
    for (const SignalName& known : signalNames) {
        if (known.number == signal) {
            return std::string{known.name};
        }
    }
    return "signal " + std::to_string(signal);
}

} // namespace opensu::app
