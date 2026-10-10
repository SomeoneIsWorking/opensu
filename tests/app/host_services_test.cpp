// The machine's services: a hidden run reads Bluetooth and the backlight and changes nothing, and
// never runs a power command; a normal run reaches the real verbs through the runner.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "host_services.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

struct Rig {
    std::vector<std::string> ran;
    std::vector<std::string> spawned;
    std::filesystem::path root{std::filesystem::path{OPENSU_TEST_SCRATCH} / "host-services"};

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
    host::LineRunner lineRunner() {
        return
            [this](const std::string& program, const std::vector<std::string>&, std::string_view) {
                ran.push_back(program + " <line>");
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
        app::HostServices::over(rig.runner(), rig.lineRunner(), rig.spawner(), rig.root, true);
    expect(!services.power->perform(host::PowerAction::ShutDown).empty(), "power is refused");
    expect(!services.power->perform(host::PowerAction::Suspend).empty(), "sleep is refused");
    expect(!services.session->switchToDesktop().empty(), "switching to the desktop is refused");
    expect(!services.session->switchToSession().empty(), "switching to session mode is refused");
    expect(services.session->install("hunter2").kind == host::InstallResult::Kind::Failed,
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
        app::HostServices::over(rig.runner(), rig.lineRunner(), rig.spawner(), rig.root, false);
    expect(services.power->perform(host::PowerAction::Restart).empty(), "restart is accepted");
    expect(ranAny(rig, "systemctl reboot"), "restart runs systemctl reboot");
}

void aBacklightIsOnlyThereWhenTheMachineHasOne() {
    Rig rig;
    std::filesystem::remove_all(rig.root);
    std::filesystem::create_directories(rig.root);
    expect(!app::HostServices::over(rig.runner(), rig.lineRunner(), rig.spawner(), rig.root, false)
                .backlight,
           "no device, no backlight");
    const std::filesystem::path device = rig.root / "panel";
    std::filesystem::create_directories(device);
    std::ofstream{device / "max_brightness"} << "100\n";
    std::ofstream{device / "brightness"} << "40\n";
    app::HostServices services =
        app::HostServices::over(rig.runner(), rig.lineRunner(), rig.spawner(), rig.root, true);
    expect(services.backlight && services.backlight->percent() == 40, "a hidden run reads it");
    expect(services.backlight->setPercent(10).empty() && services.backlight->percent() == 10,
           "a change is only held in memory");
    expect(!ranAny(rig, "SetBrightness"), "no write reached logind");
}

} // namespace

int main() {
    hiddenRunsChangeNothing();
    normalRunsReachTheSystem();
    aBacklightIsOnlyThereWhenTheMachineHasOne();
    std::printf("host_services: all checks passed\n");
    return 0;
}
