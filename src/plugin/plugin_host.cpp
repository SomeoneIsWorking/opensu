#include "plugin_host.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

#include "lucent/log.h"

namespace opensu::plugin {
namespace {

namespace fs = std::filesystem;

/// The largest `plugin.json` read.
constexpr std::size_t manifestLimit = std::size_t{64} << 10U;

std::optional<std::string> readSmallFile(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    std::string text(manifestLimit + 1, '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(in.gcount()));
    if (text.size() > manifestLimit) {
        return std::nullopt;
    }
    return text;
}

} // namespace

PluginHost::PluginHost(Locations locations) : locations_{std::move(locations)} {
    discover(locations_.bundled, true);
    discover(locations_.user, false);
}

PluginHost::~PluginHost() = default;

void PluginHost::discover(const fs::path& root, bool bundled) {
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (const fs::directory_entry& entry : fs::directory_iterator{root, ec}) {
        if (entry.is_directory(ec) && fs::exists(entry.path() / "plugin.json", ec)) {
            dirs.push_back(entry.path());
        }
    }
    std::ranges::sort(dirs);
    for (const fs::path& dir : dirs) {
        PluginInfo info;
        info.dir = dir;
        info.bundled = bundled;
        info.manifest.id = dir.filename().string();
        info.manifest.name = info.manifest.id;
        const std::optional<std::string> text = readSmallFile(dir / "plugin.json");
        ManifestRead read =
            text ? parseManifest(*text) : ManifestRead{std::nullopt, "plugin.json cannot be read"};
        if (read.manifest) {
            info.manifest = std::move(*read.manifest);
            if (info.manifest.api != apiVersion) {
                read.error = "written for API " + std::to_string(info.manifest.api) +
                             "; this openSU runs API " + std::to_string(apiVersion);
            } else if (find(info.manifest.id) != nullptr) {
                read.error = bundled ? "another bundled plugin has this id"
                                     : "a bundled plugin has this id, and cannot be replaced";
            }
        }
        if (!read.error.empty()) {
            info.state = PluginInfo::State::Refused;
            info.problem = read.error;
            lucent::warn("plugins", "{}: refused: {}", dir.string(), read.error);
        } else {
            const Permissions permissions{info.manifest, locations_.tokens};
            info.readPaths = permissions.readPaths();
            info.hosts = permissions.hosts();
        }
        plugins_.push_back(std::move(info));
    }
}

PluginInfo* PluginHost::find(const std::string& id) {
    const auto found = std::ranges::find_if(plugins_, [&id](const PluginInfo& info) {
        return info.state != PluginInfo::State::Refused && info.manifest.id == id;
    });
    return found != plugins_.end() ? &*found : nullptr;
}

void PluginHost::start(const std::string& id) {
    PluginInfo* info = find(id);
    if (info == nullptr || info->state != PluginInfo::State::Off) {
        return;
    }
    info->state = PluginInfo::State::Loading;
    info->problem.clear();
    info->faults = 0;
    lucent::info("plugins", "starting {} {}", id, info->manifest.version);
    auto worker = std::make_unique<PluginWorker>(
        PluginWorker::Setup{info->manifest, info->dir,
                            Permissions{info->manifest, locations_.tokens}, locations_.limits},
        pool_, web_, outbox_);
    if (haveGames_) {
        worker->refresh(games_);
    }
    workers_[id] = std::move(worker);
}

void PluginHost::stop(const std::string& id) {
    workers_.erase(id);
    if (PluginInfo* info = find(id); info != nullptr) {
        info->state = PluginInfo::State::Off;
        info->problem.clear();
        info->faults = 0;
        info->reported.reset();
        info->listed.reset();
        info->matched = 0;
    }
}

void PluginHost::refresh(const std::vector<library::Game>& games) {
    games_ = games;
    haveGames_ = true;
    for (const auto& [id, worker] : workers_) {
        worker->refresh(games_);
    }
}

std::optional<PluginHost::Update> PluginHost::take() {
    std::vector<WorkerNews> news = outbox_.take();
    if (news.empty()) {
        return std::nullopt;
    }
    Update update;
    for (WorkerNews& item : news) {
        PluginInfo* info = find(item.plugin);
        // News from a worker stopped since is stale.
        if (info == nullptr || !workers_.contains(item.plugin)) {
            continue;
        }
        switch (item.kind) {
        case WorkerNews::Kind::Loaded:
            info->state = PluginInfo::State::Running;
            break;
        case WorkerNews::Kind::Report:
            info->faults = 0;
            info->problem.clear();
            info->listed = item.listed;
            info->matched = item.matched;
            if (item.playTime) {
                info->reported = item.playTime->size();
                update.playTime[item.plugin] = std::move(*item.playTime);
            }
            break;
        case WorkerNews::Kind::Fault:
            ++info->faults;
            info->problem = item.message;
            break;
        case WorkerNews::Kind::Stopped:
            info->state = PluginInfo::State::Stopped;
            info->problem = item.message;
            update.stopped.emplace_back(item.plugin, item.message);
            break;
        }
    }
    return update;
}

bool PluginHost::waitIdle(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (const auto& [id, worker] : workers_) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0 || !worker->waitIdle(left)) {
            return false;
        }
    }
    return true;
}

} // namespace opensu::plugin
