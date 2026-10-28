#include "game_end.hpp"

#include <string>

namespace opensu::app {

EndNotice noticeOf(const GameEnd& end) {
    if (!end.runnerError.empty()) {
        return {end.runnerError, true};
    }
    if (const std::string crash = crashName(end.exitStatus); !crash.empty()) {
        return {end.title + " crashed (" + crash + ")", true};
    }
    if (end.cancelled) {
        return {end.title + " launch cancelled", false};
    }
    if (!end.failure.empty()) {
        return {end.failure, true};
    }
    return {end.title + " closed", false};
}

} // namespace opensu::app
