#include "launch_progress.hpp"

namespace opensu::launch {
namespace {

std::string percent(double fraction) {
    return std::to_string(static_cast<int>(fraction * 100.0)) + "%";
}

} // namespace

std::string describe(const LaunchProgress& progress) {
    switch (progress.stage) {
    case LaunchProgress::Stage::WaitingForSteam:
        return "Waiting for Steam to sign in";
    case LaunchProgress::Stage::Updating:
        return "Updating · " + percent(progress.fraction);
    case LaunchProgress::Stage::Installing:
        return "Installing · " + percent(progress.fraction);
    case LaunchProgress::Stage::Starting:
        return "Starting";
    case LaunchProgress::Stage::Preparing:
        return progress.task.empty() ? "Starting" : progress.task;
    case LaunchProgress::Stage::Loading:
        return "Loading";
    }
    return "Starting";
}

} // namespace opensu::launch
