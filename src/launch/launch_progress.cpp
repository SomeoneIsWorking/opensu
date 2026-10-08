#include "launch_progress.hpp"

namespace iideck::launch {

std::string describe(const LaunchProgress& progress) {
    switch (progress.stage) {
    case LaunchProgress::Stage::WaitingForSteam:
        return "Waiting for Steam to sign in";
    case LaunchProgress::Stage::Updating:
        return "Updating · " + std::to_string(static_cast<int>(progress.fraction * 100.0)) + "%";
    case LaunchProgress::Stage::Starting:
        return "Starting";
    case LaunchProgress::Stage::Loading:
        return "Loading";
    }
    return "Starting";
}

} // namespace iideck::launch
