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

bool inUnit(ProcessId /*pid*/, std::string_view /*unit*/) {
    return false;
}

} // namespace opensu::launch::procfs
