#include "monitor.hpp"

#include "raylib.h"

namespace iideck::session {

std::optional<Output> readMonitor() {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1, 1, "iideck monitor");
    if (!IsWindowReady()) {
        return std::nullopt;
    }
    const int monitor = GetCurrentMonitor();
    const Output output{GetMonitorWidth(monitor), GetMonitorHeight(monitor),
                        GetMonitorRefreshRate(monitor)};
    CloseWindow();
    if (output.width <= 0 || output.height <= 0) {
        return std::nullopt;
    }
    return output;
}

} // namespace iideck::session
