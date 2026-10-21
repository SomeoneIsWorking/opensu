#include "integration_switch.hpp"

#include <utility>

#include "launch/steam_gate.hpp"
#include "lucent/log.h"

namespace opensu::app {

IntegrationSwitch::IntegrationSwitch(const IntegrationsInForce& integrations, Targets targets,
                                     Hooks hooks)
    : integrations_{integrations}, targets_{targets}, hooks_{std::move(hooks)},
      applied_{integrations.current()} {
}

IntegrationSwitch::~IntegrationSwitch() {
    if (steamWorker_.joinable()) {
        steamWorker_.join();
    }
}

void IntegrationSwitch::guard() {
    const bool guards = integrations_.guardsSteam();
    targets_.pads.guardSteam(guards);
    targets_.focus.closeBigPicture(guards);
}

void IntegrationSwitch::begin() {
    const settings::Integrations& now = integrations_.current();
    lucent::info("integrations",
                 "in force: steam={} steamPads={} epic={} gog={} emulators={} plugins={}",
                 settings::label(now.steam), settings::key(now.steamPads), now.epic, now.gog,
                 now.emulators, now.plugins);
    applied_ = now;
    guard();
    moveSteam();
}

void IntegrationSwitch::apply() {
    const settings::Integrations& now = integrations_.current();
    if (now == applied_) {
        return;
    }
    const settings::Integrations was = std::exchange(applied_, now);
    if (now.plugins != was.plugins) {
        targets_.plugins.setActive(now.plugins);
    }
    if (now.emulators != was.emulators) {
        targets_.emulators.setStandalone(now.emulators);
        hooks_.emulatorsChanged();
    }
    const library::CatalogStores stores = integrations_.catalogStores();
    if (now.steam != was.steam || now.epic != was.epic || now.gog != was.gog ||
        now.emulators != was.emulators) {
        lucent::info("integrations", "the catalog reads steam={} epic={} gog={} emulators={}",
                     stores.steam, stores.epic, stores.gog, stores.standaloneEmulators);
        targets_.catalog.replace(hooks_.catalog(stores));
    }
    guard();
    if (now.steam != was.steam || now.steamPads != was.steamPads) {
        moveSteam();
    }
}

void IntegrationSwitch::service() {
    // Until it is logged on, Steam's login and update windows are the player's to answer.
    const bool hides =
        integrations_.guardsSteam() && targets_.steam.state() == launch::SteamState::Ready;
    if (hidesSteamWindows_ == hides) {
        return;
    }
    hidesSteamWindows_ = hides;
    hooks_.hideSteamWindows(hides);
}

void IntegrationSwitch::moveSteam() {
    if (steamWorker_.joinable()) {
        steamWorker_.join();
    }
    std::optional<steam::Client::Mode> mode = integrations_.steamMode();
    if (mode && !hooks_.steamInstalled()) {
        lucent::info("steam", "no Steam install found; nothing to start");
        mode.reset();
    }
    const bool all = integrations_.current().steamPads == settings::SteamPads::All;
    const bool keepsPads =
        mode == steam::Client::Mode::Isolated && !all && targets_.steam.startsClient();
    steamWorker_ = std::jthread{[&client = targets_.steam, &controllers = targets_.controllers,
                                 mode, all, keepsPads] {
        client.shutdown();
        steam::SandboxDevices devices{.allInput = all};
        devices.inputNodes = controllers.keepVirtualPads(keepsPads ? steam::isolatedPadSlots : 0);
        if (mode) {
            client.start(*mode, devices);
        }
    }};
}

} // namespace opensu::app
