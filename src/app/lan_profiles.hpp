// lan_profiles — player profiles across paired openSUs, on the shell's loop. As the owner: a paired
// openSU's ask for a copy of a profile here, put to the player as a question (Decline focused,
// Allow), the way pairing asks. As the asker: Settings › LAN › Profiles on <name>, that openSU's
// profiles, each copied with Copy: asked of it, waited for, and once allowed kept here as a profile
// with the same id linked with it, whose saves are then synced from it. Unlink drops a profile's
// link on both, each keeping its profile. The calls run off the loop through
// `LanService::askPeers`.
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "lan/profile_copies.hpp"
#include "lan/wire.hpp"
#include "lan_service.hpp"
#include "profiles/profile_book.hpp"
#include "save_sync/save_sync.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LanProfiles {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// What the pages show changed.
        std::function<void()> refresh;
    };

    LanProfiles(LanService& lan, profiles::ProfileBook& profiles, save_sync::SaveSync& saves,
                gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds, Hooks hooks)
        : lan_{lan}, profiles_{profiles}, saves_{saves}, navigator_{navigator}, sounds_{sounds},
          hooks_{std::move(hooks)} {
    }

    /// The page listing `peerId`'s profiles.
    [[nodiscard]] static std::string pageOf(const std::string& peerId);
    [[nodiscard]] static bool owns(const std::string& id);
    /// The page `id` opens asks that openSU for its profiles.
    void opening(const std::string& id);
    /// A peer's page; nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> subPage(const std::string& id) const;
    /// Acts on row `id`: Copy asks for that profile.
    void run(const std::string& id);
    /// Drops the link of `profileId` with `peerId` on both, and says so.
    void unlink(const std::string& profileId, const std::string& peerId);
    /// Shows the owner's question as the asks stand, and takes what the calls brought back. Every
    /// frame, main loop only.
    void service();

  private:
    /// A peer's profiles as last asked for.
    struct Listing {
        bool asking{false};
        std::string error;
        std::vector<lan::wire::ListedProfile> profiles;
    };
    /// What the calls off the loop brought back.
    struct Arrived {
        std::mutex mutex;
        std::vector<std::pair<std::string, Listing>> listings;
        /// Copies allowed (with the profile) or refused (with why), by peer.
        struct Copy {
            std::string peerId;
            std::optional<lan::wire::ListedProfile> profile;
            std::string refusal;
        };
        std::vector<Copy> copies;
        /// What to tell the player, and whether it is a refusal.
        std::vector<std::pair<std::string, bool>> said;
    };

    void ask(const std::string& peerId);
    void copy(const std::string& peerId, const lan::wire::ListedProfile& profile);
    void keepCopy(const Arrived::Copy& copy);
    void serviceQuestion();
    void answerQuestion(bool allow);
    void closeQuestion();

    LanService& lan_;
    profiles::ProfileBook& profiles_;
    save_sync::SaveSync& saves_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    std::map<std::string, Listing, std::less<>> listings_;
    /// The copy being waited for, by peer and profile id; empty when none.
    std::string waitingPeer_;
    std::string waitingProfile_;
    std::shared_ptr<Arrived> arrived_{std::make_shared<Arrived>()};
    /// The question asked now, and its dialog's title while the dialog is open.
    std::optional<lan::ProfileCopies::Question> asked_;
    std::string questionTitle_;
};

} // namespace opensu::app
