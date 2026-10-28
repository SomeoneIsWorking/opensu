#include "host_services.hpp"

#include <string>

namespace opensu::app {
HostServices HostServices::detect(const config::Config& config, bool hidden) {
    return over(host::systemRunner(), host::systemLineRunner(),
                host::systemSpawner(config.executablePath), "/sys/class/backlight", hidden,
                host::SessionPaths::forExecutable(config.executable,
                                                  host::DesktopRequest{config.desktopRequest},
                                                  config.session + "-compositor.scope"));
}

HostServices HostServices::over(const host::Runner& run, const host::LineRunner& runLine,
                                const host::Spawner& spawn,
                                const std::filesystem::path& backlightRoot, bool hidden,
                                host::SessionPaths paths) {
    HostServices services;
    services.power = hidden ? host::makeRefusingPower(std::string{hiddenRunRefusal})
                            : host::makeLogindPower(run);
    services.session = host::makeSddmSessionMode(run, runLine, std::move(paths));
    if (hidden) {
        services.session = host::makeReadOnlySessionMode(std::move(services.session),
                                                         std::string{hiddenRunRefusal});
    }
    services.bluetooth = host::makeBluez(run, spawn);
    services.backlight = host::makeLogindBacklight(run, backlightRoot);
    if (hidden) {
        services.bluetooth = host::makeReadOnlyBluetooth(std::move(services.bluetooth),
                                                         std::string{hiddenRunRefusal});
        if (services.backlight) {
            services.backlight = host::makeReadOnlyBacklight(std::move(services.backlight));
        }
    }
    return services;
}

} // namespace opensu::app
