#include "host_path.hpp"

#include <algorithm>

namespace opensu::fileio {

std::filesystem::path pathOnHost(const std::string& text) {
#ifdef _WIN32
    std::filesystem::path path{std::u8string{text.begin(), text.end()}};
    path.make_preferred();
    return path;
#else
    std::string path = text;
    if (path.size() >= 2 && (path[0] == 'Z' || path[0] == 'z') && path[1] == ':') {
        path.erase(0, 2);
    }
    std::ranges::replace(path, '\\', '/');
    return std::filesystem::path{path};
#endif
}

} // namespace opensu::fileio
