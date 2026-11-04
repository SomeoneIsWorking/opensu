#include "emulator_running.hpp"

#include "process_tree.hpp"

namespace opensu::launch {

std::string runningRefusal(const library::saves::EmulatorSaves& saves) {
    if (saves.processHint.empty() || !ProcessTree::anyMatches(saves.processHint)) {
        return {};
    }
    return "Close " + saves.emulatorName + " first: it is running";
}

} // namespace opensu::launch
