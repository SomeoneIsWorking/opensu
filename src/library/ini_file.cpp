#include "ini_file.hpp"

#include <fstream>

namespace opensu::library {
namespace {

constexpr std::string_view blank = " \t\r";

std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(blank);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blank) - first + 1);
}

} // namespace

std::optional<std::string> iniValue(const std::filesystem::path& file, std::string_view section,
                                    std::string_view key) {
    std::ifstream in{file};
    std::string line;
    bool inSection = false;
    while (std::getline(in, line)) {
        const std::string_view text = trimmed(line);
        if (text.starts_with("[")) {
            inSection = text.size() == section.size() + 2 && text.ends_with("]") &&
                        text.substr(1, section.size()) == section;
            continue;
        }
        const std::size_t equals = text.find_first_of("=:");
        if (inSection && equals != std::string_view::npos &&
            trimmed(text.substr(0, equals)) == key) {
            return std::string{trimmed(text.substr(equals + 1))};
        }
    }
    return std::nullopt;
}

} // namespace opensu::library
