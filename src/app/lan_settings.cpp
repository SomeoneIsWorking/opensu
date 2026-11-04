#include "lan_settings.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "lan/dns_sd.hpp"
#include "lan/peer_book.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view nameId = "lan.name";
constexpr std::string_view pairPrefix = "lan.pair.";
constexpr std::string_view forgetPrefix = "lan.forget.";
constexpr std::string_view savesPrefix = "lan.saves.";
constexpr std::string_view nameCardId = "lan-name";

std::string rowOf(std::string_view prefix, const std::string& peerId) {
    return std::string{prefix} + peerId;
}

} // namespace

ui::SettingsCategory LanSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "LAN", {}};
    const lan::LanNode* node = lan_.node();
    page.rows.push_back(ui::makeRow(std::string{nameId}, RowKind::Action, "Name",
                                    "How other openSUs on the LAN list this one", lan_.name()));
    if (node == nullptr) {
        page.rows.push_back(ui::makeRow("lan.status", RowKind::Info, "Not running",
                                        "LAN could not start; see the log", "Off"));
        return page;
    }
    const lan::LanNode::Status status = node->status();
    if (!status.problem.empty()) {
        page.rows.push_back(ui::makeRow("lan.status", RowKind::Info, "Not found by others",
                                        status.problem, "Not available"));
    } else {
        page.rows.push_back(
            ui::makeRow("lan.status", RowKind::Info, "Found on the LAN",
                        "As " + status.name + ", port " + std::to_string(status.port), "On"));
    }

    const std::vector<lan::Peer> paired = node->paired();
    const std::vector<lan::Sighting> seen = node->sightings();
    page.rows.push_back(ui::makeHeading("Found here"));
    std::size_t unpaired = 0;
    for (const lan::Sighting& sighting : seen) {
        if (std::ranges::find(paired, sighting.id, &lan::Peer::id) != paired.end()) {
            continue;
        }
        ++unpaired;
        page.rows.push_back(ui::makeRow(rowOf(pairPrefix, sighting.id), RowKind::Action,
                                        sighting.name, "At " + sighting.address, "Pair"));
    }
    if (unpaired == 0) {
        page.rows.push_back(ui::makeRow("lan.found.none", RowKind::Info, "No other openSUs found",
                                        "An openSU on this network with LAN on shows here", ""));
    }

    page.rows.push_back(ui::makeHeading("Paired"));
    for (const lan::Peer& peer : paired) {
        const bool online = std::ranges::find(seen, peer.id, &lan::Sighting::id) != seen.end();
        page.rows.push_back(ui::makeRow(
            rowOf(forgetPrefix, peer.id), RowKind::Action, peer.name,
            online ? "On the LAN; its games are in Library" : "Not on the LAN now", "Remove"));
        page.rows.push_back(ui::makeToggle(
            rowOf(savesPrefix, peer.id), "Share saves with " + peer.name,
            "Saves of the games on both go to whichever played last; it must share them too",
            peer.sharesSaves));
    }
    if (paired.empty()) {
        page.rows.push_back(ui::makeRow("lan.paired.none", RowKind::Info, "None yet",
                                        "Pair with an openSU found here to list its games", ""));
    }
    return page;
}

bool LanSettings::owns(const std::string& id) {
    return id == nameId || id.starts_with(pairPrefix) || id.starts_with(forgetPrefix) ||
           id.starts_with(savesPrefix);
}

void LanSettings::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    if (id == nameId) {
        askName();
        return;
    }
    if (id.starts_with(pairPrefix)) {
        lan_.pair(id.substr(pairPrefix.size()));
        return;
    }
    const lan::LanNode* node = lan_.node();
    if (node == nullptr) {
        return;
    }
    const bool shares = id.starts_with(savesPrefix);
    const std::string peerId = id.substr(shares ? savesPrefix.size() : forgetPrefix.size());
    const std::vector<lan::Peer> paired = node->paired();
    const auto peer = std::ranges::find(paired, peerId, &lan::Peer::id);
    if (peer == paired.end()) {
        return;
    }
    if (shares) {
        lan_.shareSaves(peerId, !peer->sharesSaves);
        return;
    }
    sounds_.play(audio::Effect::Open);
    navigator_.push(gpu2d::ui::confirm(
        "Remove " + peer->name + "?",
        "This openSU forgets the pairing, and tells " + peer->name +
            " to forget it too. Its games leave Library. Pairing again shows a new code.",
        "Remove", [this, peerId] {
            lan_.forget(peerId);
        }));
}

void LanSettings::askName() {
    draft_ = lan_.name();
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(ui::Entry{
        .id = std::string{nameCardId},
        .title = "Name this openSU",
        .note = "Other openSUs on the LAN list this one, and its games, under this name.",
        .label = "Name",
        .placeholder = lan::machineName(),
        .secret = false,
        .mostBytes = lan::longestName,
        .text = [this]() -> const std::string& {
            return draft_;
        },
        .changed =
            [this](std::string text) {
                draft_ = std::move(text);
            },
        .submitted =
            [this] {
                closeName();
                const std::string chosen = lan::shownName(draft_);
                preferences_.values().lanName = chosen == lan::machineName() ? "" : chosen;
                preferences_.save();
                lan_.rename();
                changed_();
            },
        .cancelled =
            [this] {
                closeName();
            }}));
    keyboard_.open();
}

void LanSettings::closeName() {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == nameCardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

} // namespace opensu::app
