#include "save_sharing.hpp"

#include <utility>

#include "launch/emulator_running.hpp"
#include "lucent/log.h"
#include "save_sync/folder_share.hpp"
#include "save_sync/share_target.hpp"
#include "save_sync/webdav_share.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view shareGroup = "share";

std::string secretOf(const secrets::SecretStore& secrets, std::string_view name) {
    std::string error;
    std::optional<secrets::SecretStore::Found> found = secrets.read(name, error);
    if (!error.empty()) {
        lucent::warn("saves", "the secret {} cannot be read: {}", name, error);
    }
    return found ? std::move(found->value) : std::string{};
}

/// "nas.local" for "https://nas.local:8443/dav/".
std::string hostOf(const std::string& url) {
    std::string origin = net::originOf(url);
    if (const std::size_t scheme = origin.find("://"); scheme != std::string::npos) {
        origin.erase(0, scheme + 3);
    }
    origin = origin.substr(0, origin.find('/'));
    if (const std::size_t port = origin.rfind(':');
        port != std::string::npos && origin.find(']', port) == std::string::npos) {
        origin.erase(port);
    }
    return origin.empty() ? std::string{"WebDAV"} : origin;
}

} // namespace

SaveSharing::SaveSharing(save_sync::SaveSync::Setup setup, secrets::SecretStore secrets,
                         std::function<void()> changed)
    : secrets_{std::move(secrets)}, changed_{std::move(changed)} {
    setup.changed = [news = news_] {
        news->store(true);
    };
    setup.inUse = [playing = playing_](const library::Game& game,
                                       const library::saves::EmulatorSaves& kept) {
        {
            const std::lock_guard lock{playing->mutex};
            if (playing->gameId == game.id) {
                return "Close " + game.title + " first: it is running";
            }
        }
        return launch::runningRefusal(kept);
    };
    sync_ = std::make_unique<save_sync::SaveSync>(std::move(setup));
}

void SaveSharing::share(const settings::Settings& settings) {
    std::unique_ptr<save_sync::SaveShare> share;
    std::string name;
    switch (settings.saveShare) {
    case settings::SaveShareKind::Off:
        break;
    case settings::SaveShareKind::Folder:
        if (!settings.saveShareFolder.empty()) {
            share = std::make_unique<save_sync::FolderShare>(settings.saveShareFolder);
            const std::filesystem::path& folder = settings.saveShareFolder;
            name = (folder.has_filename() ? folder : folder.parent_path()).filename().string();
        }
        break;
    case settings::SaveShareKind::WebDav:
        if (!settings.saveShareUrl.empty()) {
            share = std::make_unique<save_sync::WebDavShare>(
                web_, settings.saveShareUrl, settings.saveShareUser,
                secretOf(secrets_, webDavPasswordSecret));
            name = hostOf(settings.saveShareUrl);
        }
        break;
    }
    if (share == nullptr) {
        sync_->setTargets(std::string{shareGroup}, {});
        return;
    }
    const std::string id = "share:" + share->where();
    lucent::info("saves", "sharing saves with {} at {}", name, share->where());
    const auto target = std::make_shared<save_sync::ShareTarget>(
        id, name, std::move(share), secretOf(secrets_, sharePassphraseSecret));
    sync_->setTargets(std::string{shareGroup}, [target] {
        return std::vector<std::shared_ptr<save_sync::SaveTarget>>{target};
    });
}

void SaveSharing::know(const std::vector<library::Game>& games,
                       const library::roms::EmulatorCatalog& catalog) {
    sync_->know(games, catalog);
}

bool SaveSharing::before(const library::Game& game, std::function<void()> launch) {
    if (held_ || !sync_->sharing(game)) {
        return false;
    }
    held_ = Held{.ticket = sync_->prepare(game), .launch = std::move(launch)};
    lucent::info("saves", "{}: syncing saves before it starts", game.title);
    return true;
}

void SaveSharing::playing(const library::Game* game) {
    const std::lock_guard lock{playing_->mutex};
    playing_->gameId = game != nullptr ? game->id : std::string{};
}

void SaveSharing::ended(const library::Game& game) {
    if (sync_->sharing(game)) {
        sync_->sync(game);
    }
}

void SaveSharing::look(const library::Game& game) {
    sync_->look(game);
}

void SaveSharing::syncNow(const library::Game& game) {
    sync_->sync(game);
}

void SaveSharing::keep(const library::Game& game, const std::string& targetId, bool theirs) {
    sync_->keep(game, targetId, theirs);
}

std::vector<save_sync::TargetSaves> SaveSharing::standings(const std::string& gameId) const {
    return sync_->standings(gameId);
}

bool SaveSharing::syncing(const std::string& gameId) const {
    return sync_->busy(gameId);
}

void SaveSharing::service() {
    if (news_->exchange(false)) {
        changed_();
    }
    if (!held_ || !sync_->prepared(held_->ticket)) {
        return;
    }
    const Held held = *std::exchange(held_, std::nullopt);
    held.launch();
}

} // namespace opensu::app
