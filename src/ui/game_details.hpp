// game_details — what a game's details page says (A on its tile, Y, or Details in its menu): its
// title, the store or system it is from, its cover, the facts known about it and the actions on
// it as rows. opensu's own; iiSU shows a game's details only on the second screen of a
// dual-display device. Pure, tested without a window.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game.hpp"
#include "setting_row.hpp"

namespace opensu::ui {

/// The ids of the page's launch button and action rows.
inline constexpr std::string_view detailsLaunchId = "details.launch";
inline constexpr std::string_view detailsUpdateId = "details.update";
inline constexpr std::string_view detailsDownloadId = "details.download";
inline constexpr std::string_view detailsEmulatorId = "details.emulator";
inline constexpr std::string_view detailsSettingsId = "details.settings";
inline constexpr std::string_view detailsWinePrefixId = "details.winePrefix";
inline constexpr std::string_view detailsHomeId = "details.home";
inline constexpr std::string_view detailsHiddenId = "details.hidden";
inline constexpr std::string_view detailsUninstallId = "details.uninstall";
inline constexpr std::string_view detailsRenameId = "details.rename";
inline constexpr std::string_view detailsChangeFileId = "details.changeFile";
inline constexpr std::string_view detailsRemoveId = "details.remove";
inline constexpr std::string_view detailsMatchId = "details.match";
inline constexpr std::string_view detailsArtworkId = "details.artwork";
inline constexpr std::string_view detailsControlsId = "details.controls";
inline constexpr std::string_view detailsSavesId = "details.saves";
inline constexpr std::string_view detailsSyncSavesId = "details.syncSaves";
inline constexpr std::string_view detailsStartFreshId = "details.startFresh";
inline constexpr std::string_view detailsLinkedId = "details.linked";
/// A Play with row's id: this and the paired openSU's id.
inline constexpr std::string_view detailsPlayWithPrefix = "details.playWith:";
inline constexpr std::string_view detailsBackId = "details.back";
/// The Emulator choice's option while the game has no emulator picked.
inline constexpr std::string_view noEmulator = "None";
/// The Wine prefix choice's options, the shared one first.
inline constexpr std::string_view sharedPrefix = "Shared";
inline constexpr std::string_view ownPrefix = "Own";

/// Everything the page shows for one game.
struct GameDetails {
    std::string gameId;
    std::string title;
    /// The store or system the game is from.
    std::string badge;
    /// The cover, else the wide picture; empty for none.
    std::filesystem::path art;
    /// The wide picture faded behind the page when the cover is a portrait; empty for none.
    std::filesystem::path hero;
    /// The first button: Play, or Install for a game that is not installed.
    std::string launch;
    /// What is known about the game, as facts.
    std::vector<SettingsRow> facts;
    /// The rows that act on it, after the launch button.
    std::vector<SettingsRow> actions;

    bool operator==(const GameDetails&) const = default;
};

/// When the game was last played, in words: "Never played", "3 days ago", or the date past a month.
[[nodiscard]] std::string
lastPlayedText(const std::optional<std::chrono::system_clock::time_point>& at,
               std::chrono::system_clock::time_point now);

/// The emulator a ROM that cannot start can download: its pick when that is missing and openSU
/// can download it, else the first such emulator of its system; null for none.
[[nodiscard]] const library::EmulatorOption* emulatorToDownload(const library::Game& game);

/// The game's settings as the page shows them.
struct GameSettingsView {
    /// Where the game's emulator comes from ("Automatic (romset)", "From Super Nintendo"), added
    /// to the Emulator fact and under the Emulator choice; empty for none.
    std::string emulatorSource;
    /// After the Emulator fact: the value of each setting a layer keeps and where it comes from.
    std::vector<SettingsRow> facts;
    /// What the Game settings row reads ("Inherited", "2 custom settings"); nothing when the game
    /// has nothing to set.
    std::optional<std::string> gameSettings;
    /// What the Copy saves row reads, the emulators keeping a save of the game ("Eden, Ryujinx");
    /// empty, and no row, while fewer than two do.
    std::string copySaves;
    /// How its saves stand with the openSUs sharing them ("In sync with Desk", "Conflict with
    /// Desk"), the Saves fact, and what its row does ("Sync saves now"); both empty, and no fact or
    /// row, while no openSU shares them.
    std::string sharedSaves;
    std::string syncSaves;
    /// What the Resume fact reads while Save and close left the game a resume state ("Resumes
    /// from 2026-11-04 18:09"), with the Start fresh row; empty, and neither, while there is none.
    std::string resume;
    /// Whether the game can run on the link core: the Play linked row.
    bool linked{false};
    /// The paired openSUs to play it with, one Play with row each.
    struct Partner {
        std::string id;
        std::string name;
    };
    std::vector<Partner> partners;
};

/// What the page says about the facts' setting `label` reading `value`.
[[nodiscard]] SettingsRow detailsFact(std::string label, std::string value);

/// Where the player put a game.
struct GameMarks {
    /// Whether the player hid it.
    bool hidden{false};
    /// Whether it is on Home.
    bool onHome{false};
};

/// What the page shows for `game`, as `marks` place it. `download` is what the Download row reads:
/// the running download's progress, or empty. `ownWinePrefix` is whether a Windows install runs in
/// a Wine prefix of its own.
[[nodiscard]] GameDetails detailsOf(const library::Game& game, const GameMarks& marks,
                                    std::chrono::system_clock::time_point now,
                                    const std::string& download = {}, bool ownWinePrefix = false,
                                    const GameSettingsView& settings = {});

/// The fact labelled `label`, or null.
[[nodiscard]] const SettingsRow* factOf(const GameDetails& details, std::string_view label);
/// The action row `id`, or null.
[[nodiscard]] const SettingsRow* actionOf(const GameDetails& details, std::string_view id);

} // namespace opensu::ui
