#include "read_whole.hpp"

#include <fstream>
#include <iterator>

namespace opensu::fileio {

std::optional<std::string> readWhole(const std::filesystem::path& file, std::string& error) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (in.bad()) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    return bytes;
}

} // namespace opensu::fileio
