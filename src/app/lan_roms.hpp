// lan_roms — ROM import over the LAN on the shell's loop, both ways. As a host: the catalog's ROMs
// and the BIOS files its runner has for their consoles, offered to paired peers (`share`). As a
// guest: Settings › LAN › ROMs on <host>, the host's ROMs by console with their sizes, picked a
// game or a whole console at a time, with the BIOS files this openSU's runner lacks for those
// consoles, and Import, which copies them into this openSU's first ROM folder it may write (asking
// for one when there is none) through the ROM folders' owner (`library::roms::RomTree`), showing
// each game's progress and the total; the catalog reads them as each one arrives.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "lan/rom_import.hpp"
#include "lan/rom_shelf.hpp"
#include "lan/wire.hpp"
#include "lan_service.hpp"
#include "library/core_bios.hpp"
#include "library/game.hpp"
#include "library/rom_tree.hpp"
#include "path_editor.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LanRoms {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The ROM folders the catalog reads now.
        std::function<std::vector<std::filesystem::path>()> romFolders;
        /// Keeps `folder` with the player's ROM folders, which the catalog reads from then on.
        std::function<void(const std::filesystem::path& folder)> addRomFolder;
        /// The BIOS the runner here takes for a console; nothing when it is not known.
        std::function<std::optional<library::roms::CoreBios>(std::string_view system)> bios;
        /// The BIOS files offered to paired openSUs for the console `system`.
        std::function<std::vector<library::roms::SharedBios>(std::string_view system)> biosToShare;
        /// The runner's BIOS folder.
        std::function<std::filesystem::path()> biosFolder;
        /// ROMs arrived: the catalog reads them.
        std::function<void()> reloadCatalog;
        /// What the pages show changed.
        std::function<void()> refresh;
        /// Whether copies of a game of `store` (`library::key`) can be taken here at all.
        std::function<bool(std::string_view store)> takesCopy;
        /// Where a paired host's install goes here; nothing, with `why`, when it cannot be taken.
        std::function<std::optional<std::filesystem::path>(const lan::wire::InstallCopy& install,
                                                           std::string& why)>
            copyFolder;
        /// Has the store here take over `game`'s copy in `folder`, built for `platform`: empty when
        /// it did or started to, else why not; nothing while another install runs.
        std::function<std::optional<std::string>(const library::Game& game,
                                                 const std::filesystem::path& folder,
                                                 std::string_view platform)>
            adopt;
    };

    /// `tree` is the one the import places with (`importSetup`); `home` is where a folder chooser
    /// starts.
    LanRoms(LanService& lan, PathEditor& paths, std::shared_ptr<const library::roms::RomTree> tree,
            std::filesystem::path home, Hooks hooks)
        : lan_{lan}, paths_{paths}, tree_{std::move(tree)}, home_{std::move(home)},
          hooks_{std::move(hooks)} {
    }

    /// What imports are staged in and placed with, for `LanService::Places`.
    [[nodiscard]] static lan::RomImport::Setup
    importSetup(std::filesystem::path staging, std::shared_ptr<const library::roms::RomTree> tree);

    /// Offers the ROMs among `games` to paired peers, with their consoles' BIOS files.
    void share(const std::vector<library::Game>& games);

    /// The page listing `peerId`'s ROMs.
    [[nodiscard]] static std::string pageOf(const std::string& peerId);
    [[nodiscard]] static bool owns(const std::string& id);
    /// Where an import from Library starts: the one paired host's page, or a page choosing among
    /// them when there are several; nothing when none is paired.
    [[nodiscard]] std::optional<std::string> entry() const;
    /// The page `id` opens: a host's page asks that host for its ROMs.
    void opening(const std::string& id);
    /// A host's page, or one of its consoles; nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> subPage(const std::string& id) const;
    /// Acts on row `id`: a game or console turns over, Import imports, Cancel stops.
    void run(const std::string& id);
    /// The import's news and the listings that arrived. Every frame, main loop only.
    void service();
    /// Copies `game`, a paired host's game, here: asks the host what it offers, then imports the
    /// ROM listed as it, with the BIOS files its console lacks here, or the install listed as it
    /// into the folder its store here keeps it in, which that store then takes over.
    void copyHere(const library::Game& game);
    /// Whether `game` can be copied here and how that stands: nothing when it cannot; empty while
    /// it is not being copied; "Asking <host>", the import's progress, or "Taking over".
    [[nodiscard]] std::optional<std::string> copyState(const library::Game& game) const;
    /// The imports as the control channel reports them: "title state done/total" each, `; `
    /// between.
    [[nodiscard]] std::string glance() const;
    /// The news the import's thread left; any thread.
    void progressed() noexcept {
        news_->store(true);
    }

  private:
    /// A host's ROMs as last asked for.
    struct Listing {
        bool asking{false};
        std::string error;
        std::vector<lan::wire::RomConsole> consoles;
        std::vector<lan::wire::InstallCopy> installs;
    };
    /// What the player picked from a host.
    struct Picks {
        /// Games by `gameKey`.
        std::set<std::string> games;
        /// BIOS files the player turned off, and those turned on that are off by default.
        std::set<std::string> biosOff;
        std::set<std::string> biosOn;
    };
    /// A BIOS file offered with the picked games.
    struct OfferedBios {
        std::string system;
        lan::wire::RomFile file;
        bool picked{false};
    };

    [[nodiscard]] static std::string gameKey(std::uint32_t root, const std::string& entry);
    /// The import item of game `key` from `peerId`.
    [[nodiscard]] static std::string itemId(const std::string& peerId, const std::string& key);
    /// A game asked to be copied here, waiting for its host's listing.
    struct Wanted {
        std::string peerId;
        std::string hostGameId;
        std::string title;
    };
    /// Imports `wanted` from its host's listing, which has just arrived.
    void copyListed(const Wanted& wanted);
    /// An install being copied here: where it goes, and the game its store here takes over.
    struct InstallCopy {
        std::string peerId;
        lan::wire::InstallCopy install;
        std::filesystem::path folder;
    };
    /// Imports `copy`'s files, which its host just listed.
    void copyFiles(const std::string& remoteId, const InstallCopy& copy,
                   const std::vector<lan::wire::RomFile>& files);
    /// Hands each finished install copy to its store here.
    void adoptCopies();
    /// The paired hosts, each opening its page.
    [[nodiscard]] ui::SettingsCategory hostsPage() const;
    [[nodiscard]] ui::SettingsCategory hostPage(const std::string& peerId) const;
    [[nodiscard]] ui::SettingsCategory consolePage(const std::string& peerId,
                                                   const std::string& system) const;
    /// The BIOS files `peerId` has that the runner here lacks for the consoles `picks` has games
    /// of.
    [[nodiscard]] std::vector<OfferedBios> offeredBios(const std::string& peerId,
                                                       const Picks& picks) const;
    void ask(const std::string& peerId);
    /// Imports `picks` from `peerId`, asking for a folder first when none can be written.
    void startImport(const std::string& peerId, const Picks& picks);
    void importInto(const std::string& peerId, const std::filesystem::path& folder,
                    const Picks& picks);
    /// The folder `system`'s games go in under `folder`: one already there for it, else `host`'s.
    [[nodiscard]] std::string consoleFolder(const std::filesystem::path& folder,
                                            const std::string& system,
                                            const std::string& host) const;

    LanService& lan_;
    PathEditor& paths_;
    std::shared_ptr<const library::roms::RomTree> tree_;
    std::filesystem::path home_;
    Hooks hooks_;
    std::map<std::string, Listing, std::less<>> listings_;
    std::map<std::string, Picks, std::less<>> picks_;
    /// Copies to this device by the remote game's id: waiting for the listing, and the import
    /// item each one became.
    std::map<std::string, Wanted, std::less<>> wanted_;
    std::map<std::string, std::string, std::less<>> copies_;
    /// Install copies by remote game id: waiting for their file lists, then by import item
    /// waiting to be taken over.
    std::map<std::string, InstallCopy, std::less<>> listingFiles_;
    std::map<std::string, InstallCopy, std::less<>> adopting_;
    std::shared_ptr<std::atomic<bool>> news_{std::make_shared<std::atomic<bool>>(false)};
    std::chrono::steady_clock::time_point shown_{};
    /// How many imports were done when last looked, so each new one reloads the catalog.
    std::size_t done_{0};
    /// Listings the asking thread finished, taken on the loop.
    struct Arrived {
        std::mutex mutex;
        std::vector<std::pair<std::string, Listing>> listings;
        /// Install file lists by remote game id, or why there is none.
        std::vector<
            std::tuple<std::string, std::optional<std::vector<lan::wire::RomFile>>, std::string>>
            files;
    };
    std::shared_ptr<Arrived> arrived_{std::make_shared<Arrived>()};
};

} // namespace opensu::app
