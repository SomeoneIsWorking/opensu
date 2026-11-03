// The machine's services: a hidden run reads Bluetooth and the backlight and changes nothing, and
// never runs a power command; a normal run reaches the real verbs through the runner.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include "host_services.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

struct Rig {
    std::vector<std::string> ran;
    std::vector<std::string> spawned;
    test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "host-services"};
    std::filesystem::path root{scratch.path()};

    host::Runner runner() {
        return [this](const std::string& program, const std::vector<std::string>& args) {
            std::string line = program;
            for (const std::string& arg : args) {
                line += " " + arg;
            }
            ran.push_back(line);
            return std::optional<launch::Captured>{launch::Captured{}};
        };
    }
    host::Spawner spawner() {
        return [this](const std::string& program, const std::vector<std::string>&) {
            spawned.push_back(program);
            return std::unique_ptr<host::Holder>{};
        };
    }
};

bool ranAny(const Rig& rig, const std::string& word) {
    return std::ranges::any_of(rig.ran, [&](const std::string& line) {
        return line.find(word) != std::string::npos;
    });
}

void hiddenRunsChangeNothing() {
    Rig rig;
    app::HostServices services =
        app::HostServices::over(rig.runner(), rig.spawner(), rig.root, true);
    expect(!services.power->perform(host::PowerAction::ShutDown).empty(), "power is refused");
    expect(!services.power->perform(host::PowerAction::Suspend).empty(), "sleep is refused");
    expect(!services.session->switchToDesktop().empty(), "switching to the desktop is refused");
    expect(!services.session->switchToSession().empty(), "switching to session mode is refused");
    expect(services.session->install().kind == host::InstallResult::Kind::Failed,
           "installing session mode is refused");
    expect(!services.session->installed(), "but whether it is installed can be read");
    expect(!services.bluetooth->startDiscovery().empty(), "a scan is refused");
    expect(!services.bluetooth->pair("/d/x").empty(), "pairing is refused");
    expect(!services.bluetooth->connect("/d/x").empty(), "connecting is refused");
    expect(!services.bluetooth->remove("/d/x").empty(), "forgetting is refused");
    expect(!services.bluetooth->setPowered(false).empty(), "switching off is refused");
    expect(rig.spawned.empty(), "no scan program was started");
    expect(!ranAny(rig, "systemctl") && !ranAny(rig, "loginctl") && !ranAny(rig, "sudo") &&
               !ranAny(rig, "busctl --user call org.kde") && !ranAny(rig, "Pair") &&
               !ranAny(rig, "Connect") && !ranAny(rig, "RemoveDevice"),
           "no change reached the system");
}

void normalRunsReachTheSystem() {
    Rig rig;
    app::HostServices services =
        app::HostServices::over(rig.runner(), rig.spawner(), rig.root, false);
    expect(services.power->perform(host::PowerAction::Restart).empty(), "restart is accepted");
    expect(ranAny(rig, "systemctl reboot"), "restart runs systemctl reboot");
}

void aBacklightIsOnlyThereWhenTheMachineHasOne() {
    Rig rig;
    expect(!app::HostServices::over(rig.runner(), rig.spawner(), rig.root, false).backlight,
           "no device, no backlight");
    const std::filesystem::path device = rig.root / "panel";
    std::filesystem::create_directories(device);
    std::ofstream{device / "max_brightness"} << "100\n";
    std::ofstream{device / "brightness"} << "40\n";
    app::HostServices services =
        app::HostServices::over(rig.runner(), rig.spawner(), rig.root, true);
    expect(services.backlight && services.backlight->percent() == 40, "a hidden run reads it");
    expect(services.backlight->setPercent(10).empty() && services.backlight->percent() == 10,
           "a change is only held in memory");
    expect(!ranAny(rig, "SetBrightness"), "no write reached logind");
}

void sessionModeIsFoundThroughTheConfig() {
    Rig rig;
    const std::filesystem::path bin = rig.root / "bin";
    std::filesystem::create_directories(bin);
    // A stand-in for pkexec that notes what polkit would see of its caller, then has the password
    // dialog dismissed.
    const std::filesystem::path seen = rig.root / "pkexec-saw";
    std::ofstream{bin / "pkexec"} << "#!/bin/sh\nprintf '%s\\n' \"$PPID\" \"$*\" > '"
                                  << seen.string() << "'\nexit 126\n";
    std::filesystem::permissions(bin / "pkexec", std::filesystem::perms::owner_all);
    config::Config config;
    config.program = rig.root / "opensu";
    config.runtimeDir = rig.root / "run";
    config.desktopRequest = config.runtimeDir / "desktop";
    config.executablePath = {bin};
    config.session = "s";
    const host::SessionPaths paths = app::HostServices::sessionPaths(config);
    expect(paths.pkexec == bin / "pkexec" && paths.stage == config.runtimeDir / "session-install" &&
               paths.program == config.program,
           "pkexec comes from the config's PATH and the stage from its runtime dir");
    app::HostServices hidden = app::HostServices::detect(config, true);
    const host::InstallResult refused = hidden.session->install();
    expect(refused.kind == host::InstallResult::Kind::Failed &&
               refused.message == app::hiddenRunRefusal &&
               std::filesystem::is_regular_file(paths.stage / "install-session.sh"),
           "a hidden run stages the root step and refuses to run it");
    app::HostServices services = app::HostServices::detect(config, false);
    expect(services.session->install().kind == host::InstallResult::Kind::Cancelled,
           "a dismissed password dialog is a cancelled install");
    std::ifstream saw{seen};
    std::string parent;
    std::string args;
    std::getline(saw, parent);
    std::getline(saw, args);
    // polkit asks the agent of pkexec's parent's session, so the parent must be openSU itself.
    expect(parent == std::to_string(::getpid()), "pkexec is openSU's own child");
    expect(args == "/bin/sh " + (paths.stage / "install-session.sh").string(),
           "pkexec runs the staged installer");
}

} // namespace

int main() try {
    hiddenRunsChangeNothing();
    normalRunsReachTheSystem();
    aBacklightIsOnlyThereWhenTheMachineHasOne();
    sessionModeIsFoundThroughTheConfig();
    std::printf("host_services: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
