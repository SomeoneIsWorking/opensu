#include "size_text.hpp"

#include <array>
#include <cstdio>

namespace opensu::ui {
namespace {

constexpr double unit = 1024.0;

} // namespace

std::string sizeText(std::uint64_t bytes) {
    constexpr std::array units{"B", "KB", "MB", "GB", "TB"};
    auto size = static_cast<double>(bytes);
    std::size_t at = 0;
    while (size >= unit && at + 1 < units.size()) {
        size /= unit;
        ++at;
    }
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), at == 0 ? "%.0f %s" : "%.1f %s", size, units[at]);
    return text.data();
}

} // namespace opensu::ui
