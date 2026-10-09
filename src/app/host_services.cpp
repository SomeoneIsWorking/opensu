#include "host_services.hpp"

namespace opensu::app {
namespace {

constexpr const char* hiddenReason = "changes are off in a hidden run";

} // namespace

HostServices HostServices::detect(const config::Config& config, bool hidden) {
    return over(host::systemRunner(), host::systemSpawner(config.executablePath),
                "/sys/class/backlight", hidden,
                host::SessionExit{config.executablePath, config.loginSessionId});
}

HostServices HostServices::over(const host::Runner& run, const host::Spawner& spawn,
                                const std::filesystem::path& backlightRoot, bool hidden,
                                const host::SessionExit& exit) {
    HostServices services;
    services.power =
        hidden ? host::makeRefusingPower(hiddenReason) : host::makeLogindPower(run, exit);
    services.bluetooth = host::makeBluez(run, spawn);
    services.backlight = host::makeLogindBacklight(run, backlightRoot);
    if (hidden) {
        services.bluetooth =
            host::makeReadOnlyBluetooth(std::move(services.bluetooth), hiddenReason);
        if (services.backlight) {
            services.backlight = host::makeReadOnlyBacklight(std::move(services.backlight));
        }
    }
    return services;
}

} // namespace opensu::app
