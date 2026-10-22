// What the integration choices put in force: nothing until chosen, Steam's mode by the session.
#include "settings/integrations.hpp"

#include <array>
#include <cstdio>

#include "ui/check.hpp"

namespace {

using opensu::library::Source;
using opensu::settings::IntegrationChoices;
using opensu::settings::Integrations;
using opensu::settings::SteamMode;
using opensu::settings::SteamUse;
using opensu::test::expect;

/// The modes a Linux build's Steam client has, and a build's whose Steam is the player's own.
constexpr std::array linuxModes{SteamMode::Normal, SteamMode::Managed, SteamMode::Isolated};
constexpr std::array normalOnly{SteamMode::Normal};

void nothingChosen() {
    const Integrations none = Integrations::inForce(std::nullopt, false, linuxModes);
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
    const Integrations desktop = Integrations::inForce(chosen, false, linuxModes);
    expect(desktop.steam == SteamUse::Normal, "Steam defaults to Normal on a desktop");
    expect(desktop.epic && !desktop.gog && !desktop.emulators && desktop.plugins,
           "each switch is as chosen");
    expect(desktop.lists(Source::Steam) && desktop.lists(Source::Epic) &&
               !desktop.lists(Source::Gog),
           "the stores listed follow the switches");
    expect(Integrations::inForce(chosen, true, linuxModes).steam == SteamUse::Managed,
           "Steam defaults to Managed in a login session");

    IntegrationChoices managed = chosen;
    managed.steamMode = SteamMode::Managed;
    expect(Integrations::inForce(managed, false, linuxModes).steam == SteamUse::Managed,
           "a picked mode is in force");
    IntegrationChoices normal = chosen;
    normal.steamMode = SteamMode::Normal;
    expect(Integrations::inForce(normal, true, linuxModes).steam == SteamUse::Managed,
           "Normal is not offered in a login session");
    IntegrationChoices isolated = chosen;
    isolated.steamMode = SteamMode::Isolated;
    const Integrations sandboxed = Integrations::inForce(isolated, false, linuxModes);
    expect(sandboxed.steam == SteamUse::Isolated &&
               sandboxed.steamPads == opensu::settings::SteamPads::VirtualOnly,
           "Isolated gives Steam openSU's virtual pads by default");
    isolated.steamPads = opensu::settings::SteamPads::All;
    expect(Integrations::inForce(isolated, true, linuxModes).steamPads ==
               opensu::settings::SteamPads::All,
           "a picked controller choice is in force, in a login session too");
    managed.steamPads = opensu::settings::SteamPads::All;
    expect(Integrations::inForce(managed, false, linuxModes).steamPads ==
               opensu::settings::SteamPads::VirtualOnly,
           "only Isolated takes the controller choice");
    normal.steam = false;
    expect(Integrations::inForce(normal, false, linuxModes).steam == SteamUse::Off,
           "Steam off is off whatever its mode");
}

void modesOffered() {
    expect(Integrations::modesOffered(false, linuxModes) ==
               std::vector<SteamMode>{SteamMode::Normal, SteamMode::Managed, SteamMode::Isolated},
           "a desktop offers Normal, Managed and Isolated");
    expect(Integrations::modesOffered(true, linuxModes) ==
               std::vector<SteamMode>{SteamMode::Managed, SteamMode::Isolated},
           "a login session offers Managed and Isolated");
    expect(Integrations::modesOffered(false, normalOnly) ==
               std::vector<SteamMode>{SteamMode::Normal},
           "a build whose Steam is the player's own offers Normal alone");
    IntegrationChoices managed{.steam = true, .steamMode = SteamMode::Managed};
    expect(Integrations::inForce(managed, false, normalOnly).steam == SteamUse::Normal,
           "a mode the build lacks falls back to Normal");
}

} // namespace

int main() {
    nothingChosen();
    choicesInForce();
    modesOffered();
    std::printf("integrations: all checks passed\n");
    return 0;
}
