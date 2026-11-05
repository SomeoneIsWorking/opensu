#include "relative_path.hpp"

#include <algorithm>

namespace opensu::fileio {

bool staysInside(std::string_view path) {
    if (path.empty()) {
        return true;
    }
    if (path.front() == '/' ||
        path.find_first_of(std::string_view{"\\:\0", 3}) != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view step = path.substr(start, end - start);
        if (step.empty() || step == "." || step == "..") {
            return false;
        }
        start = end + 1;
    }
    return true;
}

} // namespace opensu::fileio
