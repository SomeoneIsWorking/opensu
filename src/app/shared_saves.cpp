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

using Standing = save_sync::TargetSaves::Standing;

/// The conflict the pick asks about, shared by its choices so copying one cannot throw.
struct Conflict {
    library::Game game;
    std::string targetId;
};

/// Which peer's standing the fact names: the one most worth the player's attention.
constexpr std::array shownFirst{Standing::Conflict,  Standing::NewerThere, Standing::Failed,
                                Standing::NewerHere, Standing::NotShared,  Standing::Offline,
                                Standing::InSync};

std::string factOf(const save_sync::TargetSaves& saves) {
    const std::string& peer = saves.targetName;
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
        return peer + " cannot be reached";
    case Standing::NotShared:
        return peer + " does not share saves with this openSU";
    case Standing::Failed:
        break;
    }
    return "Not synced with " + peer + ": " + saves.problem;
}

/// "2026-11-04 18:09 · 8.2 KB", the time alone where the size is not known (Epic's cloud), or
/// "No save" for a side without one.
std::string sideText(const save_sync::SaveStatus& status) {
    if (status.hash.empty()) {
        return "No save";
    }
    const std::chrono::system_clock::time_point at{std::chrono::seconds{status.modified}};
    const std::string moment = ui::momentText(at);
    return status.bytes == 0 ? moment : moment + " · " + ui::sizeText(status.bytes);
}

std::optional<save_sync::TargetSaves>
firstShown(const std::vector<save_sync::TargetSaves>& standings) {
    for (const Standing standing : shownFirst) {
        const auto found =
            std::ranges::find(standings, standing, &save_sync::TargetSaves::standing);
        if (found != standings.end()) {
            return *found;
        }
    }
    return std::nullopt;
}

} // namespace

SharedSaves::View SharedSaves::view(const library::Game& game) const {
    const std::optional<save_sync::TargetSaves> shown = firstShown(saves_.standings(game.id));
    if (!shown) {
        return {};
    }
    if (saves_.syncing(game.id)) {
        return View{.fact = "Syncing with " + shown->targetName, .action = "Sync saves now"};
    }
    return View{.fact = factOf(*shown),
                .action = shown->standing == Standing::Conflict ? "Pick which save to keep"
                                                                : "Sync saves now"};
}

void SharedSaves::act(const library::Game& game) {
    const std::optional<save_sync::TargetSaves> shown = firstShown(saves_.standings(game.id));
    if (!shown || saves_.syncing(game.id)) {
        return;
    }
    if (shown->standing != Standing::Conflict || !shown->theirs) {
        saves_.syncNow(game);
        return;
    }
    const std::string peer = shown->targetName;
    const auto keep = [this, conflict = std::make_shared<const Conflict>(Conflict{
                                 .game = game, .targetId = shown->targetId})](bool theirs) {
        saves_.keep(conflict->game, conflict->targetId, theirs);
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
