#include "install_log.hpp"

#include <algorithm>
#include <charconv>

namespace opensu::library::install_log {
namespace {

constexpr std::string_view progressMarker = "= Progress: ";

} // namespace

std::string_view trimmed(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.remove_suffix(1);
    }
    return line;
}

std::optional<double> progress(std::string_view line) {
    const std::size_t at = line.find(progressMarker);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view number = line.substr(at + progressMarker.size());
    double percent = 0.0;
    const auto [end, error] =
        std::from_chars(number.data(), number.data() + number.size(), percent);
    if (error != std::errc{} || end == number.data() + number.size() ||
        (*end != '%' && *end != ' ')) {
        return std::nullopt;
    }
    return std::clamp(percent / 100.0, 0.0, 1.0);
}

std::optional<std::string> loggedError(std::string_view line) {
    line = trimmed(line);
    for (const std::string_view level : {"ERROR: ", "CRITICAL: "}) {
        // "[cli] ERROR: message": the logger's name in brackets, then the level.
        const std::size_t at = line.find(level);
        if (line.starts_with('[') && at != std::string_view::npos && line.find(']') + 2 == at) {
            return std::string{line.substr(at + level.size())};
        }
    }
    return std::nullopt;
}

} // namespace opensu::library::install_log
