// integration_switch — puts what `IntegrationsInForce` holds into effect, at start and whenever
// the player changes it: the stores the catalog reads (the catalog is rebuilt and read again),
// whether standalone emulators are searched and downloaded, whether Lua plugins run, the Steam
// client in its mode (started, attached or stopped on a worker, as Managed's shutdown can take
// seconds), and the pad and focus guards only Managed asks for.
#pragma once

#include <functional>
#include <optional>
#include <thread>

#include "emulator_downloads.hpp"
#include "focus_follow.hpp"
#include "integrations_in_force.hpp"
#include "library/catalog.hpp"
#include "library/catalog_loader.hpp"
#include "pad_owner.hpp"
#include "plugin_service.hpp"
#include "steam/client.hpp"

namespace opensu::app {

class IntegrationSwitch {
  public:
    struct Targets {
        steam::Client& steam;
        library::CatalogLoader& catalog;
        PluginService& plugins;
        EmulatorDownloads& emulators;
        PadOwner& pads;
        FocusFollow& focus;
    };
    struct Hooks {
        /// The catalog for `stores`, built from the run's configuration.
        std::function<library::Catalog(const library::CatalogStores&)> catalog;
        /// Whether a Steam install is on this machine; without one Steam is not started.
        std::function<bool()> steamInstalled;
        /// Standalone emulators came or went: the screens that list emulators read them again.
        std::function<void()> emulatorsChanged;
    };

    IntegrationSwitch(const IntegrationsInForce& integrations, Targets targets, Hooks hooks);
    ~IntegrationSwitch();
    IntegrationSwitch(const IntegrationSwitch&) = delete;
    IntegrationSwitch& operator=(const IntegrationSwitch&) = delete;

    /// Applies what is in force when the shell starts: the guards and the Steam client.
    void begin();
    /// Applies a change: only what differs from what was applied last. Main loop only.
    void apply();

  private:
    void guard();
    /// Moves the client to `mode` on the worker, after any move still under way.
    void moveSteam(std::optional<steam::Client::Mode> mode);

    const IntegrationsInForce& integrations_;
    Targets targets_;
    Hooks hooks_;
    settings::Integrations applied_;
    /// Last, so a Steam start or stop under way ends before the client is used by nothing.
    std::jthread steamWorker_;
};

} // namespace opensu::app
