#include "integration_switch.hpp"

#include <utility>

#include "launch/steam_gate.hpp"
#include "lucent/log.h"

namespace opensu::app {

IntegrationSwitch::IntegrationSwitch(const IntegrationsInForce& integrations, Targets targets,
                                     Hooks hooks)
    : integrations_{integrations}, targets_{targets}, hooks_{std::move(hooks)},
      applied_{integrations.current()} {
    steamWorker_ = std::jthread{[this](const std::stop_token& stop) {
        steamLoop(stop);
    }};
}

IntegrationSwitch::~IntegrationSwitch() {
    // The worker waits on the stop token; its thread joins as the member is destroyed.
    steamWorker_.request_stop();
    steamWake_.notify_all();
}

void IntegrationSwitch::guard() {
    targets_.pads.guardSteam(integrations_.guardsSteam());
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
    const bool guards = integrations_.guardsSteam() && !hooks_.steamWindowsWanted();
    targets_.focus.closeBigPicture(guards);
    // Until it is logged on, Steam's login and update windows are the player's to answer.
    const bool hides = guards && targets_.steam.state() == launch::SteamState::Ready;
    if (hidesSteamWindows_ == hides) {
        return;
    }
    hidesSteamWindows_ = hides;
    hooks_.hideSteamWindows(hides);
}

std::optional<steam::Client::Mode> IntegrationSwitch::steamMode() {
    std::optional<steam::Client::Mode> mode = integrations_.steamMode();
    if (mode && !hooks_.steamInstalled()) {
        lucent::info("steam", "no Steam install found; nothing to start");
        mode.reset();
    }
    return mode;
}

launch::SteamGate* IntegrationSwitch::steamInUse() {
    if (!steamMode() || !targets_.steam.startsClient()) {
        return nullptr;
    }
    return &targets_.steam;
}

void IntegrationSwitch::moveSteam() {
    const std::optional<steam::Client::Mode> mode = steamMode();
    const bool all = integrations_.current().steamPads == settings::SteamPads::All;
    const bool keepsPads =
        mode == steam::Client::Mode::Isolated && !all && targets_.steam.startsClient();
    {
        const std::lock_guard lock{steamMutex_};
        steamRequest_ = SteamMove{
            .serial = steamRequest_.serial + 1, .mode = mode, .all = all, .keepsPads = keepsPads};
    }
    steamWake_.notify_all();
}

void IntegrationSwitch::steamLoop(const std::stop_token& stop) {
    std::unique_lock lock{steamMutex_};
    std::uint64_t done = 0;
    while (true) {
        steamWake_.wait(lock, stop, [this, done] {
            return steamRequest_.serial != done;
        });
        if (stop.stop_requested()) {
            return;
        }
        const SteamMove move = steamRequest_;
        done = move.serial;
        lock.unlock();
        targets_.steam.shutdown();
        steam::SandboxDevices devices{.allInput = move.all};
        devices.inputNodes =
            targets_.controllers.keepVirtualPads(move.keepsPads ? steam::isolatedPadSlots : 0);
        if (move.mode) {
            targets_.steam.start(*move.mode, devices);
        }
        lock.lock();
    }
}

} // namespace opensu::app
