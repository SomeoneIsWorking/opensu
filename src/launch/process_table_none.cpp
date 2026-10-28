#include "process_table.hpp"

namespace opensu::launch::table {

bool available() {
    return false;
}

std::vector<ProcessId> pids() {
    return {};
}

ProcessId parentOf(ProcessId /*pid*/) {
    return -1;
}

std::string commandLine(ProcessId /*pid*/) {
    return {};
}

bool kill(ProcessId /*pid*/) {
    return false;
}

} // namespace opensu::launch::table
