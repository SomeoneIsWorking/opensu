#include "xemu_config.hpp"

#include <optional>

#include "ini_file.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view bindings{"input.bindings"};

} // namespace

std::string xemuLaunchSettings(std::string_view toml, std::string_view padGuid) {
    std::string out = withIniValue(toml, {"general", "show_welcome"}, "false");
    if (padGuid.empty()) {
        return out;
    }
    const std::string quoted = "'" + std::string{padGuid} + "'";
    for (int port = 1; port <= xemuPorts; ++port) {
        out = withIniValue(out, {std::string{bindings}, "port" + std::to_string(port)}, quoted);
    }
    lucent::info("xemu", "ports 1 to {} bound to the virtual pads, {}", xemuPorts, padGuid);
    return out;
}

std::vector<fs::path> xemuFiles(const LaunchSetup& setup) {
    return {setup.config / xemuSettingsFile};
}

std::vector<RunFile> xemuRunFiles(const LaunchSetup& setup) {
    std::optional<RunFile> run =
        runFileOf(setup.config / xemuSettingsFile, [&](std::string_view toml) {
            return xemuLaunchSettings(toml, setup.padGuid);
        });
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library
