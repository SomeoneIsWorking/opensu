#include "procfs.hpp"

namespace opensu::launch::procfs {

std::string read(ProcessId /*pid*/, std::string_view /*entry*/) {
    return {};
}

bool running(ProcessId /*pid*/) {
    return false;
}

std::string name(ProcessId /*pid*/) {
    return {};
}

std::optional<std::uint64_t> startTime(ProcessId /*pid*/) {
    return std::nullopt;
}

bool inUnit(ProcessId /*pid*/, std::string_view /*unit*/) {
    return false;
}

} // namespace opensu::launch::procfs
