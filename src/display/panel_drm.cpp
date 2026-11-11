// The kernel's DRM connectors under /sys/class/drm: `card<N>-<type>-<M>` with `status` and `edid`.
#include "panel.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace opensu::display {
namespace {

namespace fs = std::filesystem;

constexpr const char* connectors = "/sys/class/drm";

bool builtIn(const std::string& connector) {
    const std::string type = connector.substr(connector.find('-') + 1);
    return type.starts_with("eDP") || type.starts_with("LVDS") || type.starts_with("DSI");
}

bool connected(const fs::path& connector) {
    std::ifstream status{connector / "status"};
    std::string state;
    return (status >> state) && state == "connected";
}

} // namespace

Panel connectedPanel() {
    std::error_code failure;
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator{connectors, failure}) {
        const std::string name = entry.path().filename().string();
        if (name.starts_with("card") && name.find('-') != std::string::npos &&
            connected(entry.path())) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    std::stable_partition(names.begin(), names.end(), builtIn);
    if (names.empty()) {
        return Panel{};
    }
    std::ifstream file{fs::path{connectors} / names.front() / "edid", std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
    const std::vector<std::uint8_t> edid{raw.begin(), raw.end()};
    return panelOf(edid, builtIn(names.front()));
}

} // namespace opensu::display
