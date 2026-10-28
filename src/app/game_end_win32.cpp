#include "game_end.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace opensu::app {
namespace {

struct ExceptionName {
    std::uint32_t code;
    std::string_view name;
};

constexpr std::array<ExceptionName, 7> exceptionNames{{
    {0xC0000005U, "access violation"},
    {0xC00000FDU, "stack overflow"},
    {0xC0000409U, "stack buffer overrun"},
    {0xC000001DU, "illegal instruction"},
    {0xC0000094U, "integer divide by zero"},
    {0xC0000374U, "heap corruption"},
    {0x80000003U, "breakpoint"},
}};

/// The exit status of a process ended with Ctrl+C.
constexpr std::uint32_t controlCExit = 0xC000013AU;

} // namespace

std::string crashName(int exitStatus) {
    const auto code = static_cast<std::uint32_t>(exitStatus);
    // An NTSTATUS error or warning is an unhandled exception; a plain exit code is not.
    const bool exception = (code & 0xC0000000U) == 0xC0000000U || code == 0x80000003U;
    if (!exception || code == controlCExit) {
        return {};
    }
    for (const ExceptionName& known : exceptionNames) {
        if (known.code == code) {
            return std::string{known.name};
        }
    }
    return std::format("exception 0x{:08X}", code);
}

} // namespace opensu::app
