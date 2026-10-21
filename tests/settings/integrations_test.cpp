// What the integration choices put in force: nothing until chosen, Steam's mode by the session.
#include "settings/integrations.hpp"

#include <cstdio>

#include "ui/check.hpp"

namespace {

using opensu::library::Source;
using opensu::settings::IntegrationChoices;
using opensu::settings::Integrations;
using opensu::settings::SteamMode;
using opensu::settings::SteamUse;
using opensu::test::expect;

void nothingChosen() {
    const Integrations none = Integrations::inForce(std::nullopt, false);
    expect(none == Integrations{}, "nothing chosen is everything off");
    expect(none.lists(Source::Rom), "ROMs are always listed");
    expect(!none.lists(Source::Steam) && !none.lists(Source::Epic) && !none.lists(Source::Gog),
           "no store is listed");
}

void choicesInForce() {
    IntegrationChoices chosen;
    chosen.steam = true;
    chosen.epic = true;
    chosen.plugins = true;
    const Integrations desktop = Integrations::inForce(chosen, false);
    expect(desktop.steam == SteamUse::Normal, "Steam defaults to Normal on a desktop");
    expect(desktop.epic && !desktop.gog && !desktop.emulators && desktop.plugins,
           "each switch is as chosen");
    expect(desktop.lists(Source::Steam) && desktop.lists(Source::Epic) &&
               !desktop.lists(Source::Gog),
           "the stores listed follow the switches");
    expect(Integrations::inForce(chosen, true).steam == SteamUse::Managed,
           "Steam defaults to Managed in a login session");

    IntegrationChoices managed = chosen;
    managed.steamMode = SteamMode::Managed;
    expect(Integrations::inForce(managed, false).steam == SteamUse::Managed,
           "a picked mode is in force");
    IntegrationChoices normal = chosen;
    normal.steamMode = SteamMode::Normal;
    expect(Integrations::inForce(normal, true).steam == SteamUse::Managed,
           "Normal is not offered in a login session");
    IntegrationChoices isolated = chosen;
    isolated.steamMode = SteamMode::Isolated;
    const Integrations sandboxed = Integrations::inForce(isolated, false);
    expect(sandboxed.steam == SteamUse::Isolated &&
               sandboxed.steamPads == opensu::settings::SteamPads::VirtualOnly,
           "Isolated gives Steam openSU's virtual pads by default");
    isolated.steamPads = opensu::settings::SteamPads::All;
    expect(Integrations::inForce(isolated, true).steamPads == opensu::settings::SteamPads::All,
           "a picked controller choice is in force, in a login session too");
    managed.steamPads = opensu::settings::SteamPads::All;
    expect(Integrations::inForce(managed, false).steamPads ==
               opensu::settings::SteamPads::VirtualOnly,
           "only Isolated takes the controller choice");
    normal.steam = false;
    expect(Integrations::inForce(normal, false).steam == SteamUse::Off,
           "Steam off is off whatever its mode");
}

void modesOffered() {
    expect(Integrations::modesOffered(false) ==
               std::vector<SteamMode>{SteamMode::Normal, SteamMode::Managed, SteamMode::Isolated},
           "a desktop offers Normal, Managed and Isolated");
    expect(Integrations::modesOffered(true) ==
               std::vector<SteamMode>{SteamMode::Managed, SteamMode::Isolated},
           "a login session offers Managed and Isolated");
}

} // namespace

int main() {
    nothingChosen();
    choicesInForce();
    modesOffered();
    std::printf("integrations: all checks passed\n");
    return 0;
}
