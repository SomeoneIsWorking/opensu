// steam_library_settings — the Settings screen's Steam libraries category: every library Steam
// keeps, with its path, free space, games and their size, and which is the default; add a library,
// make one the default, remove an empty one (on a second press). The running client is asked off
// the loop; without it the libraries in `libraryfolders.vdf` are listed read-only.
#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "library/steam.hpp"
#include "path_editor.hpp"
#include "steam/libraries.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class SteamLibrarySettings {
  public:
    struct Sources {
        /// The running client's libraries.
        steam::Libraries& live;
        /// Whether Steam is running and can be asked.
        std::function<bool()> steamReady;
        /// The libraries Steam recorded on disk.
        std::function<std::vector<library::steam::LibraryFolder>()> recorded;
    };
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The list changed: the screen shows it again.
        std::function<void()> refresh;
    };

    SteamLibrarySettings(Sources sources, PathEditor& paths, std::filesystem::path home,
                         Hooks hooks);
    ~SteamLibrarySettings();
    SteamLibrarySettings(const SteamLibrarySettings&) = delete;
    SteamLibrarySettings& operator=(const SteamLibrarySettings&) = delete;

    static constexpr std::string_view categoryId = "steamLibraries";

    /// Reads the libraries again: from Steam in the background when it runs, else from disk.
    void reload();
    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const std::string& id, int step);
    void disarm() noexcept {
        armed_.reset();
    }
    /// Takes what the worker finished. Main loop only.
    void service();

    /// Steam's default library as the running client last said; nothing when it was not asked.
    [[nodiscard]] std::optional<std::filesystem::path> defaultLibrary() const;

  private:
    using Action = std::function<std::string(steam::Libraries&)>;

    /// Runs `action` on the worker, then reads the list again; after the running one when busy.
    void submit(Action action);
    [[nodiscard]] const steam::SteamLibrary* find(int index) const;
    void addLibrary();

    Sources sources_;
    PathEditor& paths_;
    std::filesystem::path home_;
    Hooks hooks_;

    mutable std::mutex mutex_;
    /// The running client's answer, or nothing while it was not asked or could not be.
    std::optional<std::vector<steam::SteamLibrary>> live_;
    std::string liveError_;
    std::vector<library::steam::LibraryFolder> recorded_;
    bool busy_{false};
    /// What was asked while Steam was answering, in order.
    std::vector<Action> queued_;
    bool changed_{false};
    std::vector<std::pair<std::string, bool>> said_;
    /// The library whose Remove was pressed once.
    std::optional<int> armed_;
    /// Last, so it stops before what it uses goes.
    std::jthread worker_;
};

/// `bytes` as the player reads a size: "45.2 GB".
[[nodiscard]] std::string sizeText(std::uint64_t bytes);

} // namespace opensu::app
