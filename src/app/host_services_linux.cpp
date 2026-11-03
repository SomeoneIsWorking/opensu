#include "host_services.hpp"

#include <string>

#include "launch/command.hpp"

namespace opensu::app {

host::SessionPaths HostServices::sessionPaths(const config::Config& config) {
    host::SessionPaths paths;
    paths.program = config.program;
    paths.stage = config.runtimeDir / "session-install";
    paths.pkexec = launch::resolveExecutable("pkexec", config.executablePath);
    paths.request = host::DesktopRequest{config.desktopRequest};
    paths.compositorScope = config.session + "-compositor.scope";
    return paths;
}

HostServices HostServices::detect(const config::Config& config, bool hidden) {
    return over(host::systemRunner(), host::systemSpawner(config.executablePath),
                "/sys/class/backlight", hidden, sessionPaths(config));
}

HostServices HostServices::over(const host::Runner& run, const host::Spawner& spawn,
                                const std::filesystem::path& backlightRoot, bool hidden,
                                host::SessionPaths paths) {
    HostServices services;
    services.power = hidden ? host::makeRefusingPower(std::string{hiddenRunRefusal})
                            : host::makeLogindPower(run);
    services.session = host::makeSddmSessionMode(run, std::move(paths));
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
