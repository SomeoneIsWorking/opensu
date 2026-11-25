#include "host_services.hpp"

#include <filesystem>
#include <string>

#include "launch/command.hpp"

namespace opensu::app {

host::SessionPaths HostServices::sessionPaths(const config::Config& config) {
    host::SessionPaths paths;
    paths.program = config.program;
    paths.stage = config.runtimeDir / "session-install";
    paths.pkexec = launch::resolveExecutable("pkexec", config.executablePath);
    paths.rootStepStandIn = config.rootStepStandIn;
    if (!config.sessionRootStandIn.empty()) {
        for (std::filesystem::path& dir : paths.entryDirs) {
            dir = config.sessionRootStandIn / dir.relative_path();
        }
        paths.selector = config.sessionRootStandIn / paths.selector.relative_path();
    }
    paths.request = host::DesktopRequest{config.desktopRequest};
    paths.compositorScope = config.session + "-compositor.scope";
    return paths;
}

host::RingLightHelperPaths HostServices::ringLightPaths(const config::Config& config) {
    host::RingLightHelperPaths paths;
    paths.stage = config.runtimeDir / "ring-lights-install";
    paths.pkexec = launch::resolveExecutable("pkexec", config.executablePath);
    paths.rootStepStandIn = config.rootStepStandIn;
    return paths;
}

HostServices HostServices::detect(const config::Config& config, bool hidden) {
    HostServices services = over(host::systemRunner(), host::systemSpawner(config.executablePath),
                                 "/sys/class/backlight", hidden, sessionPaths(config),
                                 ringLightPaths(config));
    services.polkit = host::startPolkitAgent(config.messageLocale, config.polkitHelperStandIn);
    return services;
}

HostServices HostServices::over(const host::Runner& run, const host::Spawner& spawn,
                                const std::filesystem::path& backlightRoot, bool hidden,
                                host::SessionPaths paths, host::RingLightHelperPaths lights) {
    HostServices services;
    const bool standInStep = !paths.rootStepStandIn.empty();
    services.power = hidden ? host::makeRefusingPower(std::string{hiddenRunRefusal})
                            : host::makeLogindPower(run);
    services.session = host::makeSddmSessionMode(run, std::move(paths));
    if (hidden) {
        services.session = host::makeReadOnlySessionMode(
            std::move(services.session), std::string{hiddenRunRefusal}, standInStep);
    }
    services.bluetooth = host::makeBluez(run, spawn);
    services.backlight = host::makeLogindBacklight(run, backlightRoot);
    services.powerProfiles = host::makePowerStationProfiles(run);
    const bool standInLights = !lights.rootStepStandIn.empty();
    services.ringLights = host::makeSudoRingLightHelper(run, std::move(lights));
    if (hidden) {
        services.ringLights = host::makeReadOnlyRingLightHelper(
            std::move(services.ringLights), std::string{hiddenRunRefusal}, standInLights);
        services.bluetooth = host::makeReadOnlyBluetooth(std::move(services.bluetooth),
                                                         std::string{hiddenRunRefusal});
        if (services.backlight) {
            services.backlight = host::makeReadOnlyBacklight(std::move(services.backlight));
        }
        services.powerProfiles = host::makeReadOnlyPowerProfiles(std::move(services.powerProfiles),
                                                                 std::string{hiddenRunRefusal});
    }
    return services;
}

} // namespace opensu::app
