#include "integration_settings.hpp"

#include <algorithm>
#include <vector>

#include "integration_options.hpp"

namespace opensu::app {
namespace {

using settings::SteamMode;
using settings::SteamPads;
using settings::SteamUse;
using ui::RowKind;

constexpr std::string_view prefix = "integrations.";
constexpr const char* modeId = "integrations.steamMode";
constexpr const char* padsId = "integrations.steamPads";
constexpr const char* alwaysId = "integrations.always";
constexpr const char* applyId = "integrations.apply";

std::string rowId(std::string_view entry) {
    return std::string{prefix} + std::string{entry};
}

std::string_view modeNote(SteamMode mode) {
    switch (mode) {
    case SteamMode::Managed:
        return "openSU's own Steam, ended with openSU";
    case SteamMode::Isolated:
        return "openSU's own Steam, kept off the desktop";
    case SteamMode::Normal:
        break;
    }
    return "Your desktop Steam, left as it is";
}

SteamMode modeOf(SteamUse use) {
    switch (use) {
    case SteamUse::Managed:
        return SteamMode::Managed;
    case SteamUse::Isolated:
        return SteamMode::Isolated;
    case SteamUse::Normal:
    case SteamUse::Off:
        break;
    }
    return SteamMode::Normal;
}

/// The entry `step` places from `now` in `offered`, wrapping; A steps forward.
template <typename T> T stepped(const std::vector<T>& offered, T now, int step) {
    const auto at = std::ranges::find(offered, now);
    const auto from = at == offered.end() ? 0 : at - offered.begin();
    const auto count = static_cast<long>(offered.size());
    const long next = (from + (step == 0 ? 1 : step) % count + count) % count;
    return offered[static_cast<std::size_t>(next)];
}

/// `offered` as a choice's options, `now` chosen.
template <typename T> ui::Options optionsOf(const std::vector<T>& offered, T now) {
    ui::Options options;
    for (const T& entry : offered) {
        options.labels.emplace_back(settings::label(entry));
    }
    const auto at = std::ranges::find(offered, now);
    options.chosen = at == offered.end() ? 0 : static_cast<std::size_t>(at - offered.begin());
    return options;
}

} // namespace

bool IntegrationSettings::applies(const std::string& id) {
    return id == applyId;
}

std::size_t IntegrationSettings::apply() {
    const std::size_t count = pendingCount();
    if (pending_) {
        integrations_.choose(*pending_);
        pending_.reset();
        changed_();
    }
    return count;
}

std::size_t IntegrationSettings::pendingCount() const {
    if (!pending_) {
        return 0;
    }
    const settings::IntegrationChoices& now = integrations_.choices();
    const settings::IntegrationChoices& edit = *pending_;
    return static_cast<std::size_t>(now.steam != edit.steam) +
           static_cast<std::size_t>(now.steamMode != edit.steamMode) +
           static_cast<std::size_t>(now.steamPads != edit.steamPads) +
           static_cast<std::size_t>(now.epic != edit.epic) +
           static_cast<std::size_t>(now.gog != edit.gog) +
           static_cast<std::size_t>(now.emulators != edit.emulators) +
           static_cast<std::size_t>(now.plugins != edit.plugins);
}

ui::SettingsCategory IntegrationSettings::page() const {
    const settings::IntegrationChoices choices = effective();
    const settings::Integrations& now = integrations_.preview(choices);
    ui::SettingsCategory page{std::string{categoryId}, "Integrations", {}};
    page.rows.push_back(ui::makeRow(alwaysId, RowKind::Info, "ROMs and libretro",
                                    "ROM folders, played on libretro", "Always on"));
    for (const IntegrationEntry& entry : integrationEntries()) {
        page.rows.push_back(ui::makeToggle(rowId(entry.id), std::string{entry.label},
                                           std::string{entry.note}, switchOf(choices, entry.id)));
        if (entry.id != "steam" || !choices.steam) {
            continue;
        }
        const SteamMode mode = modeOf(now.steam);
        // A build whose Steam has one mode has nothing to choose.
        if (integrations_.modesOffered().size() > 1) {
            page.rows.push_back(ui::makeChoice(modeId, "Steam mode", std::string{modeNote(mode)},
                                               optionsOf(integrations_.modesOffered(), mode)));
        }
        if (mode == SteamMode::Isolated) {
            page.rows.push_back(
                ui::makeChoice(padsId, "Steam sees", std::string{settings::note(now.steamPads)},
                               optionsOf(settings::Integrations::padsOffered(), now.steamPads)));
        }
    }
    const std::size_t waiting = pendingCount();
    if (waiting > 0) {
        page.rows.push_back(ui::makeRow(applyId, RowKind::Action, "Apply changes",
                                        std::to_string(waiting) +
                                            (waiting == 1 ? " change is" : " changes are") +
                                            " waiting to be applied",
                                        "Apply"));
    } else {
        page.rows.push_back(ui::makeRow(applyId, RowKind::Info, "Apply changes",
                                        "Every change is already in effect", "Saved"));
    }
    return page;
}

bool IntegrationSettings::owns(const std::string& id) {
    return id.starts_with(prefix);
}

void IntegrationSettings::run(const std::string& id, int step) {
    if (id == applyId) {
        if (step == 0) {
            apply();
        }
        return;
    }
    settings::IntegrationChoices choices = effective();
    const settings::Integrations& now = integrations_.preview(choices);
    if (id == modeId) {
        choices.steamMode = stepped(integrations_.modesOffered(), modeOf(now.steam), step);
    } else if (id == padsId) {
        choices.steamPads = stepped(settings::Integrations::padsOffered(), now.steamPads, step);
    } else if (bool* on = switchOf(choices, id.substr(prefix.size())); on != nullptr && step == 0) {
        *on = !*on;
    } else {
        return;
    }
    pending_ = choices;
}

} // namespace opensu::app
