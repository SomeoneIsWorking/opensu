#include "plugin_service.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"

namespace opensu::app {
namespace {

plugin::PluginHost::Locations locationsFor(const config::Config& environment) {
    return {.bundled = environment.assetsDir / "plugins",
            .user = environment.dataDir / "plugins",
            .tokens = {.home = environment.home,
                       .xdgData = environment.xdgDataHome,
                       .xdgConfig = environment.xdgConfigHome,
                       .xdgCache = environment.xdgCacheHome},
            .limits = {}};
}

} // namespace

PluginService::PluginService(const config::Config& environment, Preferences& preferences,
                             bool active, Hooks hooks)
    : locations_{locationsFor(environment)}, preferences_{preferences}, hooks_{std::move(hooks)} {
    if (active) {
        host_ = std::make_unique<plugin::PluginHost>(locations_);
        startChosen();
    }
}

void PluginService::setActive(bool active) {
    if (active == (host_ != nullptr)) {
        return;
    }
    if (active) {
        host_ = std::make_unique<plugin::PluginHost>(locations_);
        startChosen();
    } else {
        for (const plugin::PluginInfo& info : host_->plugins()) {
            reports_.erase(info.manifest.id);
        }
        host_.reset();
        turnedOff_ = true;
    }
    lucent::info("plugins", "Lua plugins {}", active ? "on" : "off");
    hooks_.statusChanged();
}

void PluginService::startChosen() {
    for (const plugin::PluginInfo& info : host_->plugins()) {
        if (enabled(info)) {
            host_->start(info.manifest.id);
        }
    }
}

bool PluginService::enabled(const plugin::PluginInfo& info) const {
    if (info.state == plugin::PluginInfo::State::Refused) {
        return false;
    }
    const auto& choices = preferences_.values().plugins;
    const auto found = choices.find(info.manifest.id);
    if (info.bundled) {
        return found == choices.end() || found->second.enabled;
    }
    return found != choices.end() && found->second.enabled && !needsConsent(info);
}

bool PluginService::needsConsent(const plugin::PluginInfo& info) const {
    if (info.bundled) {
        return false;
    }
    const auto& choices = preferences_.values().plugins;
    const auto found = choices.find(info.manifest.id);
    return found == choices.end() || found->second.granted != info.manifest.permissions();
}

void PluginService::setEnabled(const std::string& id, bool on) {
    if (!host_) {
        return;
    }
    const auto info = std::ranges::find_if(host_->plugins(), [&id](const plugin::PluginInfo& it) {
        return it.manifest.id == id;
    });
    if (info == host_->plugins().end() || info->state == plugin::PluginInfo::State::Refused) {
        return;
    }
    auto& choices = preferences_.values().plugins;
    if (on && info->bundled) {
        choices.erase(id);
    } else {
        choices[id] = settings::PluginChoice{.enabled = on,
                                             .granted = on ? info->manifest.permissions()
                                                           : std::vector<std::string>{}};
    }
    preferences_.save();
    if (on) {
        host_->start(id);
    } else {
        host_->stop(id);
        reports_.erase(id);
        turnedOff_ = true;
    }
    hooks_.statusChanged();
}

void PluginService::request(const std::vector<library::Game>& games) {
    if (host_) {
        host_->refresh(games);
    }
}

void PluginService::service() {
    if (std::exchange(turnedOff_, false)) {
        hooks_.playTimeChanged();
    }
    if (!host_) {
        return;
    }
    std::optional<plugin::PluginHost::Update> update = host_->take();
    if (!update) {
        return;
    }
    for (auto& [id, report] : update->playTime) {
        reports_.set(id, std::move(report));
    }
    for (const auto& [id, reason] : update->stopped) {
        const auto info = std::ranges::find_if(host_->plugins(), [&id](const auto& it) {
            return it.manifest.id == id;
        });
        std::string text = info != host_->plugins().end() ? info->manifest.name : id;
        text += " stopped: ";
        text += reason;
        hooks_.say(text, true);
    }
    if (!update->playTime.empty()) {
        hooks_.playTimeChanged();
    }
    hooks_.statusChanged();
}

} // namespace opensu::app
