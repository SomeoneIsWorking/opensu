#include "locations.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace opensu::config {
namespace {

std::filesystem::path required(std::optional<std::filesystem::path> resolved, const char* what) {
    if (!resolved) {
        throw std::runtime_error{std::string{"openSU's "} + what +
                                 " directory cannot be resolved from the environment"};
    }
    return *std::move(resolved);
}

} // namespace

Locations Locations::resolve(const lucent::platform::Environment& environment) {
    namespace platform = lucent::platform;
    return Locations{
        .configDir = required(
            platform::resolve_user_data_directory(OPENSU_LOCATION_NAME, environment), "config"),
        .dataDir =
            required(platform::resolve_data_directory(OPENSU_LOCATION_NAME, environment), "data"),
        .cacheDir =
            required(platform::resolve_cache_directory(OPENSU_LOCATION_NAME, environment), "cache"),
        .runtimeDir = required(
            platform::resolve_runtime_directory(OPENSU_LOCATION_NAME, environment), "runtime"),
    };
}

} // namespace opensu::config
