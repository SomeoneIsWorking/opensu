#include "lan_roms.hpp"

#include <algorithm>
#include <utility>

#include "lan/relative_path.hpp"
#include "library/catalog.hpp"
#include "library/rom_systems.hpp"
#include "lucent/log.h"
#include "lucent/text.h"
#include "settings/install_folders.hpp"
#include "ui/size_text.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using lan::RomImport;
using ui::RowKind;

constexpr std::string_view pagePrefix = "lanroms:";
constexpr std::string_view gamePrefix = "lanroms.game:";
constexpr std::string_view allPrefix = "lanroms.all:";
constexpr std::string_view biosPrefix = "lanroms.bios:";
constexpr std::string_view importPrefix = "lanroms.import:";
constexpr std::string_view cancelPrefix = "lanroms.cancel:";
constexpr std::string_view againPrefix = "lanroms.again:";
/// How often the pages are shown again while an import runs.
constexpr std::chrono::milliseconds showEvery{250};

/// `text` split at its first `:`; the rest empty when there is none.
std::pair<std::string, std::string> splitFirst(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return {std::string{text}, {}};
    }
    return {std::string{text.substr(0, colon)}, std::string{text.substr(colon + 1)}};
}

std::string percentOf(std::uint64_t done, std::uint64_t total) {
    return std::to_string(total == 0 ? 100 : done * 100 / total) + " %";
}

std::string valueOf(const RomImport::Progress& progress) {
    switch (progress.state) {
    case RomImport::State::Queued:
        return "Queued";
    case RomImport::State::Fetching:
        return percentOf(progress.done, progress.total);
    case RomImport::State::Waiting:
        return "Waiting · " + percentOf(progress.done, progress.total);
    case RomImport::State::Checking:
        return "Checking";
    case RomImport::State::Done:
        return "Done";
    case RomImport::State::Failed:
        return "Failed";
    }
    return {};
}

bool ended(const RomImport::Progress& progress) {
    return progress.state == RomImport::State::Done || progress.state == RomImport::State::Failed;
}

} // namespace

lan::RomImport::Setup LanRoms::importSetup(fs::path staging,
                                           std::shared_ptr<const library::roms::RomTree> tree) {
    return RomImport::Setup{
        .selfId = {},
        .staging = std::move(staging),
        .link = {},
        .place =
            [tree = std::move(tree)](const RomImport::Item& /*item*/, const RomImport::File& file,
                                     const fs::path& staged, std::string& error) {
                return tree->place(file.folder, file.place, staged, error);
            },
        .changed = {}};
}

void LanRoms::share(const std::vector<library::Game>& games) {
    lan::RomShelf::Contents contents{
        .roots = hooks_.romFolders(), .games = {}, .biosFolder = hooks_.biosFolder(), .bios = {}};
    for (const library::Game& game : games) {
        if (game.source != library::Source::Rom) {
            continue;
        }
        if (!contents.bios.contains(game.sourceId)) {
            std::vector<std::string>& files = contents.bios[game.sourceId];
            if (const std::optional<library::roms::CoreBios> bios = hooks_.bios(game.sourceId)) {
                for (const library::roms::CoreBios::File& file : bios->files) {
                    if (file.present) {
                        files.push_back(file.firmware.path);
                    }
                }
            }
        }
        contents.games.push_back(game);
    }
    lan_.shareRoms(std::move(contents));
}

std::string LanRoms::pageOf(const std::string& peerId) {
    return std::string{pagePrefix} + peerId;
}

bool LanRoms::owns(const std::string& id) {
    return id.starts_with(gamePrefix) || id.starts_with(allPrefix) || id.starts_with(biosPrefix) ||
           id.starts_with(importPrefix) || id.starts_with(cancelPrefix) ||
           id.starts_with(againPrefix);
}

std::string LanRoms::gameKey(std::uint32_t root, const std::string& entry) {
    return std::to_string(root) + ":" + entry;
}

std::string LanRoms::hostName(const std::string& peerId) const {
    if (const lan::LanNode* node = lan_.node()) {
        for (const lan::Peer& peer : node->paired()) {
            if (peer.id == peerId) {
                return peer.name;
            }
        }
    }
    return "that openSU";
}

void LanRoms::opening(const std::string& id) {
    if (!id.starts_with(pagePrefix)) {
        return;
    }
    const auto [peerId, system] = splitFirst(std::string_view{id}.substr(pagePrefix.size()));
    const auto known = listings_.find(peerId);
    if (system.empty() || known == listings_.end() || !known->second.error.empty()) {
        ask(peerId);
    }
}

void LanRoms::ask(const std::string& peerId) {
    Listing& listing = listings_[peerId];
    if (listing.asking) {
        return;
    }
    const bool asked = lan_.askRoms(
        peerId, [arrived = arrived_, peerId](std::optional<std::vector<lan::wire::RomConsole>> got,
                                             std::string error) {
            Listing answer;
            if (got) {
                answer.consoles = std::move(*got);
            } else {
                answer.error = error.empty() ? std::string{"it did not answer"} : std::move(error);
            }
            const std::lock_guard lock{arrived->mutex};
            arrived->listings.emplace_back(peerId, std::move(answer));
        });
    listing.asking = asked;
    if (!asked && lan_.node() == nullptr) {
        listing.error = "LAN is off";
    }
}

void LanRoms::service() {
    std::vector<std::pair<std::string, Listing>> listings;
    {
        const std::lock_guard lock{arrived_->mutex};
        listings.swap(arrived_->listings);
    }
    bool show = !listings.empty();
    for (auto& [peerId, listing] : listings) {
        if (listing.error.empty()) {
            lucent::info("lan", "{} offers ROMs of {} consoles", hostName(peerId),
                         listing.consoles.size());
        } else {
            lucent::warn("lan", "the ROMs of {} cannot be listed: {}", hostName(peerId),
                         listing.error);
        }
        listings_[peerId] = std::move(listing);
    }
    const auto now = std::chrono::steady_clock::now();
    if (news_->load() && now - shown_ >= showEvery) {
        news_->store(false);
        shown_ = now;
        show = true;
        if (lan::LanNode* node = lan_.node()) {
            const std::vector<RomImport::Progress> all = node->imports().progress();
            const auto done = static_cast<std::size_t>(
                std::ranges::count(all, RomImport::State::Done, &RomImport::Progress::state));
            if (done > done_) {
                hooks_.reloadCatalog();
            }
            done_ = done;
        }
    }
    if (show) {
        hooks_.refresh();
    }
}

std::vector<LanRoms::OfferedBios> LanRoms::offeredBios(const std::string& peerId) const {
    std::vector<OfferedBios> offered;
    const auto listing = listings_.find(peerId);
    const auto picks = picks_.find(peerId);
    if (listing == listings_.end() || picks == picks_.end()) {
        return offered;
    }
    std::set<std::string> systems;
    for (const lan::wire::RomConsole& console : listing->second.consoles) {
        const bool picked = std::ranges::any_of(console.games, [&](const lan::wire::RomGame& game) {
            return picks->second.games.contains(gameKey(game.root, game.entry));
        });
        if (!picked || !systems.insert(console.system).second) {
            continue;
        }
        const std::optional<library::roms::CoreBios> here = hooks_.bios(console.system);
        if (!here) {
            continue;
        }
        for (const lan::wire::RomConsole& from : listing->second.consoles) {
            if (from.system != console.system) {
                continue;
            }
            for (const lan::wire::RomFile& file : from.bios) {
                const auto wanted =
                    std::ranges::find_if(here->files, [&](const library::roms::CoreBios::File& f) {
                        return f.firmware.path == file.path && !f.present;
                    });
                if (wanted == here->files.end() ||
                    std::ranges::any_of(offered, [&](const OfferedBios& kept) {
                        return kept.file.path == file.path;
                    })) {
                    continue;
                }
                // Picked unless the core starts without it.
                const bool byDefault = here->without == library::roms::WithoutBios::Needed &&
                                       !wanted->firmware.optional;
                const bool picked = byDefault ? !picks->second.biosOff.contains(file.path)
                                              : picks->second.biosOn.contains(file.path);
                offered.push_back(OfferedBios{console.system, file, picked});
            }
        }
    }
    return offered;
}

std::optional<ui::SettingsCategory> LanRoms::subPage(const std::string& id) const {
    if (!id.starts_with(pagePrefix)) {
        return std::nullopt;
    }
    const auto [peerId, system] = splitFirst(std::string_view{id}.substr(pagePrefix.size()));
    return system.empty() ? hostPage(peerId) : consolePage(peerId, system);
}

ui::SettingsCategory LanRoms::hostPage(const std::string& peerId) const {
    const std::string host = hostName(peerId);
    ui::SettingsCategory page{pageOf(peerId), "ROMs on " + host, {}};
    const auto listing = listings_.find(peerId);
    const auto picks = picks_.find(peerId);
    if (listing == listings_.end() || listing->second.asking) {
        page.rows.push_back(ui::makeRow("lanroms.status", RowKind::Info, "Asking " + host,
                                        "For the ROMs it has", ""));
    } else if (!listing->second.error.empty()) {
        page.rows.push_back(
            ui::makeRow(std::string{againPrefix} + peerId, RowKind::Action, "Ask again",
                        "It could not list its ROMs: " + listing->second.error, ""));
    } else {
        // Consoles by key; the host may keep one console's games in more than one folder.
        std::map<std::string, std::pair<std::size_t, std::uint64_t>> consoles;
        std::map<std::string, std::size_t> picked;
        std::size_t games = 0;
        std::uint64_t bytes = 0;
        for (const lan::wire::RomConsole& console : listing->second.consoles) {
            for (const lan::wire::RomGame& game : console.games) {
                auto& [count, size] = consoles[console.system];
                ++count;
                size += game.bytes();
                if (picks != picks_.end() &&
                    picks->second.games.contains(gameKey(game.root, game.entry))) {
                    ++picked[console.system];
                    ++games;
                    bytes += game.bytes();
                }
            }
        }
        page.rows.push_back(ui::makeHeading("Consoles"));
        for (const auto& [key, facts] : consoles) {
            const std::size_t chosen = picked.contains(key) ? picked.at(key) : 0;
            page.rows.push_back(ui::makePage(
                pageOf(peerId) + ":" + key, library::roms::systemLabel(key),
                lucent::text::counted(facts.first, "game") + " · " + ui::sizeText(facts.second),
                chosen == 0 ? "" : std::to_string(chosen) + " picked"));
        }
        if (consoles.empty()) {
            page.rows.push_back(ui::makeRow("lanroms.none", RowKind::Info, "No ROMs",
                                            host + " lists no ROMs to import", ""));
        }
        const std::vector<OfferedBios> bios = offeredBios(peerId);
        if (!bios.empty()) {
            page.rows.push_back(ui::makeHeading("BIOS"));
        }
        for (const OfferedBios& file : bios) {
            page.rows.push_back(ui::makeToggle(
                std::string{biosPrefix} + peerId + ":" + file.file.path, file.file.path,
                "For " + library::roms::systemLabel(file.system) + " · " +
                    ui::sizeText(file.file.bytes) + " · the runner here lacks it",
                file.picked));
            if (file.picked) {
                bytes += file.file.bytes;
            }
        }
        if (games > 0) {
            page.rows.push_back(ui::makeRow(std::string{importPrefix} + peerId, RowKind::Action,
                                            "Import " + lucent::text::counted(games, "game"),
                                            ui::sizeText(bytes) + " into this openSU's ROM folder",
                                            ""));
        }
    }
    const lan::LanNode* node = lan_.node();
    if (node == nullptr) {
        return page;
    }
    std::vector<RomImport::Progress> imports = node->imports().progress();
    std::erase_if(imports, [&](const RomImport::Progress& progress) {
        return progress.host != peerId;
    });
    if (imports.empty()) {
        return page;
    }
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    bool running = false;
    for (const RomImport::Progress& progress : imports) {
        done += progress.done;
        total += progress.total;
        running = running || !ended(progress);
    }
    page.rows.push_back(ui::makeHeading("Importing"));
    page.rows.push_back(ui::makeRow("lanroms.total", RowKind::Info, "All",
                                    ui::sizeText(done) + " of " + ui::sizeText(total),
                                    percentOf(done, total)));
    for (const RomImport::Progress& progress : imports) {
        std::string note = progress.note;
        if (note.empty()) {
            note =
                library::roms::systemLabel(progress.system) + " · " + ui::sizeText(progress.total);
        }
        page.rows.push_back(ui::makeRow("lanroms.item:" + progress.id, RowKind::Info,
                                        progress.title, note, valueOf(progress)));
    }
    if (running) {
        page.rows.push_back(ui::makeRow(std::string{cancelPrefix} + peerId, RowKind::Action,
                                        "Stop importing", "What has not arrived is dropped", ""));
    }
    return page;
}

ui::SettingsCategory LanRoms::consolePage(const std::string& peerId,
                                          const std::string& system) const {
    ui::SettingsCategory page{
        pageOf(peerId) + ":" + system, library::roms::systemLabel(system), {}};
    const auto listing = listings_.find(peerId);
    if (listing == listings_.end()) {
        return page;
    }
    const auto picks = picks_.find(peerId);
    std::vector<const lan::wire::RomGame*> games;
    for (const lan::wire::RomConsole& console : listing->second.consoles) {
        if (console.system == system) {
            for (const lan::wire::RomGame& game : console.games) {
                games.push_back(&game);
            }
        }
    }
    std::ranges::sort(games, {}, &lan::wire::RomGame::title);
    const auto isPicked = [&](const lan::wire::RomGame& game) {
        return picks != picks_.end() &&
               picks->second.games.contains(gameKey(game.root, game.entry));
    };
    std::uint64_t bytes = 0;
    for (const lan::wire::RomGame* game : games) {
        bytes += game->bytes();
    }
    page.rows.push_back(ui::makeToggle(
        std::string{allPrefix} + peerId + ":" + system, "Whole console",
        lucent::text::counted(games.size(), "game") + " · " + ui::sizeText(bytes),
        !games.empty() && std::ranges::all_of(games, [&](const lan::wire::RomGame* game) {
            return isPicked(*game);
        })));
    page.rows.push_back(ui::makeHeading("Games"));
    for (const lan::wire::RomGame* game : games) {
        page.rows.push_back(ui::makeToggle(
            std::string{gamePrefix} + peerId + ":" + gameKey(game->root, game->entry), game->title,
            ui::sizeText(game->bytes()) +
                (game->files.size() > 1 ? " · " + lucent::text::counted(game->files.size(), "file")
                                        : ""),
            isPicked(*game)));
    }
    return page;
}

void LanRoms::run(const std::string& id) {
    const std::string_view row{id};
    if (row.starts_with(againPrefix)) {
        ask(std::string{row.substr(againPrefix.size())});
        hooks_.refresh();
        return;
    }
    if (row.starts_with(importPrefix)) {
        startImport(std::string{row.substr(importPrefix.size())});
        return;
    }
    if (row.starts_with(cancelPrefix)) {
        if (lan::LanNode* node = lan_.node()) {
            node->imports().cancel(std::string{row.substr(cancelPrefix.size())});
        }
        hooks_.refresh();
        return;
    }
    const std::string_view prefix = row.starts_with(gamePrefix)  ? gamePrefix
                                    : row.starts_with(allPrefix) ? allPrefix
                                                                 : biosPrefix;
    const auto [peerId, rest] = splitFirst(row.substr(prefix.size()));
    Picks& picks = picks_[peerId];
    if (prefix == gamePrefix) {
        if (!picks.games.erase(rest)) {
            picks.games.insert(rest);
        }
    } else if (prefix == biosPrefix) {
        for (const OfferedBios& file : offeredBios(peerId)) {
            if (file.file.path != rest) {
                continue;
            }
            if (file.picked) {
                picks.biosOff.insert(rest);
                picks.biosOn.erase(rest);
            } else {
                picks.biosOn.insert(rest);
                picks.biosOff.erase(rest);
            }
        }
    } else if (const auto listing = listings_.find(peerId); listing != listings_.end()) {
        std::vector<std::string> keys;
        for (const lan::wire::RomConsole& console : listing->second.consoles) {
            if (console.system == rest) {
                for (const lan::wire::RomGame& game : console.games) {
                    keys.push_back(gameKey(game.root, game.entry));
                }
            }
        }
        const bool all = std::ranges::all_of(keys, [&](const std::string& key) {
            return picks.games.contains(key);
        });
        for (const std::string& key : keys) {
            if (all) {
                picks.games.erase(key);
            } else {
                picks.games.insert(key);
            }
        }
    }
    hooks_.refresh();
}

void LanRoms::startImport(const std::string& peerId) {
    for (const fs::path& folder : hooks_.romFolders()) {
        if (tree_->writeRefusal(folder).empty()) {
            importInto(peerId, folder);
            return;
        }
    }
    const std::string host = hostName(peerId);
    PathEditor::Request request;
    request.title = "A ROM folder for the games from " + host;
    request.start = home_;
    request.refusal = [](const fs::path& folder) {
        return settings::refusal(folder, settings::Need::Write);
    };
    request.done = [this, peerId](const std::optional<fs::path>& folder) {
        if (!folder) {
            return;
        }
        if (const std::string refused = tree_->writeRefusal(*folder); !refused.empty()) {
            hooks_.say(refused, true);
            return;
        }
        hooks_.addRomFolder(*folder);
        importInto(peerId, *folder);
    };
    paths_.begin(std::move(request));
}

std::string LanRoms::consoleFolder(const fs::path& folder, const std::string& system,
                                   const std::string& host) const {
    for (const library::roms::RomNode& node :
         tree_->children(folder).value_or(std::vector<library::roms::RomNode>{})) {
        const library::roms::RomSystem* holds = library::roms::systemForFolder(node.name);
        if (node.folder && holds != nullptr && holds->key == system &&
            lan::staysInside(node.name)) {
            return node.name;
        }
    }
    return host;
}

void LanRoms::importInto(const std::string& peerId, const fs::path& folder) {
    lan::LanNode* node = lan_.node();
    const auto listing = listings_.find(peerId);
    const auto picks = picks_.find(peerId);
    if (node == nullptr || listing == listings_.end() || picks == picks_.end()) {
        return;
    }
    const std::string host = hostName(peerId);
    std::vector<RomImport::Item> items;
    std::map<std::string, std::string> folders;
    for (const lan::wire::RomConsole& console : listing->second.consoles) {
        const std::string key = console.system + "\n" + console.folder;
        if (!folders.contains(key)) {
            folders[key] = consoleFolder(folder, console.system, console.folder);
        }
        const std::string& mine = folders[key];
        for (const lan::wire::RomGame& game : console.games) {
            if (!picks->second.games.contains(gameKey(game.root, game.entry))) {
                continue;
            }
            RomImport::Item item{"rom:" + peerId + ":" + gameKey(game.root, game.entry),
                                 peerId,
                                 host,
                                 game.title,
                                 console.system,
                                 {}};
            for (const lan::wire::RomFile& file : game.files) {
                item.files.push_back(
                    RomImport::File{lan::wire::RomKind::Rom, game.root, file.path, file.bytes,
                                    folder, mine + file.path.substr(console.folder.size())});
            }
            items.push_back(std::move(item));
        }
    }
    const std::size_t games = items.size();
    for (const OfferedBios& file : offeredBios(peerId)) {
        if (file.picked) {
            items.push_back(RomImport::Item{
                "bios:" + peerId + ":" + file.file.path,
                peerId,
                host,
                file.file.path,
                file.system,
                {RomImport::File{lan::wire::RomKind::Bios, 0, file.file.path, file.file.bytes,
                                 hooks_.biosFolder(), file.file.path}}});
        }
    }
    lucent::info("lan", "importing {} games and {} BIOS files from {} into {}", games,
                 items.size() - games, host, tree_->label(folder));
    node->imports().add(std::move(items));
    picks->second.games.clear();
    hooks_.say("Importing " + lucent::text::counted(games, "game") + " from " + host, false);
    hooks_.refresh();
}

std::string LanRoms::glance() const {
    const lan::LanNode* node = lan_.node();
    if (node == nullptr) {
        return {};
    }
    std::string out;
    for (const RomImport::Progress& progress : node->imports().progress()) {
        if (!out.empty()) {
            out += "; ";
        }
        out += progress.title + " " + std::string{lan::label(progress.state)} + " " +
               std::to_string(progress.done) + "/" + std::to_string(progress.total);
    }
    return out;
}

} // namespace opensu::app
