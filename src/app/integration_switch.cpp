#include "integration_switch.hpp"

#include <utility>

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
    lucent::info("integrations", "in force: steam={} epic={} gog={} emulators={} plugins={}",
                 settings::label(now.steam), now.epic, now.gog, now.emulators, now.plugins);
    applied_ = now;
    guard();
    moveSteam(integrations_.steamMode());
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
    if (now.steam != was.steam) {
        moveSteam(integrations_.steamMode());
    }
}

void IntegrationSwitch::moveSteam(std::optional<steam::Client::Mode> mode) {
    if (steamWorker_.joinable()) {
        steamWorker_.join();
    }
    if (mode && !hooks_.steamInstalled()) {
        lucent::info("steam", "no Steam install found; nothing to start");
        mode.reset();
    }
    steamWorker_ = std::jthread{[&client = targets_.steam, mode] {
        client.shutdown();
        if (mode) {
            client.start(*mode);
        }
    }};
}

} // namespace opensu::app
