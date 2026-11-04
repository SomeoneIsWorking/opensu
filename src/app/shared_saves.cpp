#include "shared_saves.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "ui/moment_text.hpp"
#include "ui/size_text.hpp"

namespace opensu::app {
namespace {

using Standing = lan::PeerSaves::Standing;

/// Which peer's standing the fact names: the one most worth the player's attention.
constexpr std::array shownFirst{Standing::Conflict,  Standing::NewerThere, Standing::Failed,
                                Standing::NewerHere, Standing::NotShared,  Standing::Offline,
                                Standing::InSync};

std::string factOf(const lan::PeerSaves& saves) {
    const std::string& peer = saves.peer.name;
    switch (saves.standing) {
    case Standing::InSync:
        return "In sync with " + peer;
    case Standing::NewerThere:
        return "Newer on " + peer;
    case Standing::NewerHere:
        return "Newer here than on " + peer;
    case Standing::Conflict:
        return "Conflict with " + peer;
    case Standing::Offline:
        return peer + " is offline";
    case Standing::NotShared:
        return peer + " does not share saves with this openSU";
    case Standing::Failed:
        break;
    }
    return "Not synced with " + peer + ": " + saves.problem;
}

/// "2026-11-04 18:09 · 8.2 KB", or "No save" for a side without one.
std::string sideText(const lan::wire::SaveStatus& status) {
    if (status.hash.empty()) {
        return "No save";
    }
    const std::chrono::system_clock::time_point at{std::chrono::seconds{status.modified}};
    return ui::momentText(at) + " · " + ui::sizeText(status.bytes);
}

std::optional<lan::PeerSaves> firstShown(const std::vector<lan::PeerSaves>& standings) {
    for (const Standing standing : shownFirst) {
        const auto found = std::ranges::find(standings, standing, &lan::PeerSaves::standing);
        if (found != standings.end()) {
            return *found;
        }
    }
    return std::nullopt;
}

} // namespace

SharedSaves::View SharedSaves::view(const library::Game& game) const {
    const std::optional<lan::PeerSaves> shown = firstShown(lan_.standings(game.id));
    if (!shown) {
        return {};
    }
    if (lan_.syncing(game.id)) {
        return View{.fact = "Syncing with " + shown->peer.name, .action = "Sync saves now"};
    }
    return View{.fact = factOf(*shown),
                .action = shown->standing == Standing::Conflict ? "Pick which save to keep"
                                                                : "Sync saves now"};
}

void SharedSaves::act(const library::Game& game) {
    const std::optional<lan::PeerSaves> shown = firstShown(lan_.standings(game.id));
    if (!shown || lan_.syncing(game.id)) {
        return;
    }
    if (shown->standing != Standing::Conflict || !shown->theirs) {
        lan_.syncNow(game);
        return;
    }
    const std::string peer = shown->peer.name;
    const auto keep = [this, game, id = shown->peer.id](bool theirs) {
        lan_.keep(game, id, theirs);
    };
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{
        .label = "Cancel", .chosen = {}, .kind = gpu2d::ui::ButtonKind::Normal, .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Keep this one",
                                                .chosen =
                                                    [keep] {
                                                        keep(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = false});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Keep " + peer + "'s",
                                                .chosen =
                                                    [keep] {
                                                        keep(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = false});
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        "Which save of " + game.title + "?",
        "Both changed since they last synced.\n\nHere: " + sideText(shown->ours) + "\n" + peer +
            ": " + sideText(*shown->theirs) + "\n\nThe save replaced is backed up first.",
        std::move(choices), 1));
}

} // namespace opensu::app
