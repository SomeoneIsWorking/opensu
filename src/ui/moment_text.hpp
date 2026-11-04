// moment_text — a moment as the player reads it on a card that compares saves: the one rule Copy
// saves and shared saves use.
#pragma once

#include <chrono>
#include <string>

namespace opensu::ui {

/// "2026-04-29 18:09", in local time.
[[nodiscard]] std::string momentText(std::chrono::system_clock::time_point at);

} // namespace opensu::ui
