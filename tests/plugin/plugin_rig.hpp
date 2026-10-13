// A scratch tree of plugins and the directories their read paths name, and the shipping
// `PluginHost` over it: write a plugin, start it, refresh it over games, wait and take the news.
#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "plugin/plugin_host.hpp"
#include "ui/check.hpp"

namespace opensu::test {

inline void writeFile(const std::filesystem::path& file, const std::string& bytes) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary};
    out << bytes;
}

/// A ROM game of `system` whose file is `file`.
inline library::Game romGame(const std::string& title, const std::filesystem::path& file,
                             const std::string& system = "switch") {
    library::Game game;
    game.id = "rom:" + file.string();
    game.source = library::Source::Rom;
    game.sourceId = system;
    game.title = title;
    game.file = file;
    game.emulator = "Eden";
    return game;
}

/// Limits small enough that a runaway plugin trips them in a fraction of a second.
inline plugin::Limits testLimits() {
    return plugin::Limits{
        .memoryBytes = std::size_t{8} << 20U, .instructions = 2'000'000, .runTime{2000}};
}

class PluginRig {
  public:
    explicit PluginRig(std::filesystem::path root) : root_{std::move(root)} {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / "user");
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }
    [[nodiscard]] plugin::PathTokens tokens() const {
        return {.home = root_ / "home",
                .xdgData = root_ / "data",
                .xdgConfig = root_ / "config",
                .xdgCache = root_ / "cache"};
    }

    /// A plugin the player added, with `read` as its manifest's read groups (JSON) and `http`.
    void addPlugin(const std::string& id, const std::string& mainLua,
                   const std::string& read = "{}", const std::string& http = "[]") {
        const std::filesystem::path dir = root_ / "user" / id;
        writeFile(dir / "plugin.json",
                  R"({"id": ")" + id + R"(", "name": ")" + id +
                      R"(", "version": "1.0.0", "api": 1, "permissions": {"read": )" + read +
                      R"(, "http": )" + http + "}}");
        writeFile(dir / "main.lua", mainLua);
    }

    /// Makes the host over the user plugins and `bundled`, starts every plugin, and returns what
    /// they said while loading.
    plugin::PluginHost::Update start(const std::filesystem::path& bundled = {}) {
        host_.reset();
        host_ = std::make_unique<plugin::PluginHost>(
            plugin::PluginHost::Locations{.bundled = bundled,
                                          .user = root_ / "user",
                                          .tokens = tokens(),
                                          .limits = testLimits()});
        for (const plugin::PluginInfo& info : host_->plugins()) {
            host_->start(info.manifest.id);
        }
        return settle();
    }

    /// Refreshes over `games`, waits, and returns what came back.
    plugin::PluginHost::Update refresh(const std::vector<library::Game>& games) {
        host_->refresh(games);
        return settle();
    }

    /// Waits for every worker and takes what they said.
    plugin::PluginHost::Update settle() {
        expect(host_->waitIdle(std::chrono::seconds{20}), "the plugins settle");
        plugin::PluginHost::Update all;
        while (std::optional<plugin::PluginHost::Update> update = host_->take()) {
            for (auto& [id, report] : update->playTime) {
                all.playTime[id] = std::move(report);
            }
            all.stopped.insert(all.stopped.end(), update->stopped.begin(), update->stopped.end());
        }
        return all;
    }

    [[nodiscard]] const plugin::PluginInfo& info(const std::string& id) const {
        for (const plugin::PluginInfo& info : host_->plugins()) {
            if (info.manifest.id == id) {
                return info;
            }
        }
        fail("a plugin with that id exists");
    }

  private:
    std::filesystem::path root_;
    std::unique_ptr<plugin::PluginHost> host_;
};

} // namespace opensu::test
