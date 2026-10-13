#include "steam_library_settings.hpp"

#include <array>
#include <cstdio>
#include <system_error>

#include "settings/install_folders.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view addId = "steamLibraries.add";
constexpr std::string_view libraryId = "steamLibraries.library.";
constexpr std::string_view removeId = "steamLibraries.remove.";
constexpr double unit = 1024.0;

std::string gamesText(std::size_t count, std::uint64_t bytes) {
    return std::to_string(count) + (count == 1 ? " game, " : " games, ") + sizeText(bytes);
}

std::string spaceText(std::uint64_t freeSpace, std::uint64_t capacity) {
    return sizeText(freeSpace) + " free" +
           (capacity != 0 ? " of " + sizeText(capacity) : std::string{});
}

std::optional<int> indexIn(std::string_view id, std::string_view prefix) {
    if (!id.starts_with(prefix)) {
        return std::nullopt;
    }
    try {
        return std::stoi(std::string{id.substr(prefix.size())});
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

std::string sizeText(std::uint64_t bytes) {
    constexpr std::array units{"B", "KB", "MB", "GB", "TB"};
    auto size = static_cast<double>(bytes);
    std::size_t at = 0;
    while (size >= unit && at + 1 < units.size()) {
        size /= unit;
        ++at;
    }
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), at == 0 ? "%.0f %s" : "%.1f %s", size, units[at]);
    return text.data();
}

SteamLibrarySettings::SteamLibrarySettings(Sources sources, PathEditor& paths,
                                           std::filesystem::path home, Hooks hooks)
    : sources_{std::move(sources)}, paths_{paths}, home_{std::move(home)},
      hooks_{std::move(hooks)} {
}

SteamLibrarySettings::~SteamLibrarySettings() {
    worker_.request_stop();
}

bool SteamLibrarySettings::owns(const std::string& id) {
    return id.starts_with(std::string{categoryId} + ".");
}

void SteamLibrarySettings::reload() {
    armed_.reset();
    {
        const std::lock_guard lock{mutex_};
        recorded_ = sources_.recorded();
    }
    if (!sources_.steamReady()) {
        const std::lock_guard lock{mutex_};
        live_.reset();
        liveError_.clear();
        return;
    }
    submit({});
}

void SteamLibrarySettings::submit(Action action) {
    {
        const std::lock_guard lock{mutex_};
        if (busy_) {
            // Asked while Steam answers: done next, after a read.
            queued_.push_back(std::move(action));
            return;
        }
        busy_ = true;
    }
    worker_ = std::jthread{[this, first = std::move(action)](const std::stop_token& stop) {
        Action action = first;
        while (!stop.stop_requested()) {
            const std::string refused = action ? action(sources_.live) : std::string{};
            std::string error;
            std::optional<std::vector<steam::SteamLibrary>> libraries = sources_.live.read(error);
            const std::lock_guard lock{mutex_};
            if (!refused.empty()) {
                said_.emplace_back(refused, true);
            }
            live_ = std::move(libraries);
            liveError_ = error;
            changed_ = true;
            if (queued_.empty()) {
                busy_ = false;
                return;
            }
            action = std::move(queued_.front());
            queued_.erase(queued_.begin());
        }
    }};
}

void SteamLibrarySettings::service() {
    std::vector<std::pair<std::string, bool>> said;
    bool changed = false;
    {
        const std::lock_guard lock{mutex_};
        said = std::exchange(said_, {});
        changed = std::exchange(changed_, false);
    }
    for (const auto& [text, error] : said) {
        hooks_.say(text, error);
    }
    if (changed || !said.empty()) {
        hooks_.refresh();
    }
}

std::optional<std::filesystem::path> SteamLibrarySettings::defaultLibrary() const {
    const std::lock_guard lock{mutex_};
    if (live_) {
        for (const steam::SteamLibrary& library : *live_) {
            if (library.isDefault) {
                return library.path;
            }
        }
    }
    return std::nullopt;
}

const steam::SteamLibrary* SteamLibrarySettings::find(int index) const {
    if (!live_) {
        return nullptr;
    }
    for (const steam::SteamLibrary& library : *live_) {
        if (library.index == index) {
            return &library;
        }
    }
    return nullptr;
}

ui::SettingsCategory SteamLibrarySettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Steam libraries", {}};
    const std::lock_guard lock{mutex_};
    if (!live_) {
        const bool asking = busy_;
        page.rows.push_back(ui::makeRow(
            "steamLibraries.state", RowKind::Info, asking ? "Asking Steam" : "Steam is not running",
            asking ? ""
                   : "Read-only until openSU has started Steam" +
                         (liveError_.empty() ? std::string{} : ": " + liveError_),
            ""));
        for (const library::steam::LibraryFolder& folder : recorded_) {
            std::uint64_t bytes = 0;
            for (const auto& [app, size] : folder.apps) {
                bytes += size;
            }
            std::error_code ec;
            const std::filesystem::space_info space = std::filesystem::space(folder.path, ec);
            page.rows.push_back(
                ui::makeRow("steamLibraries.recorded", RowKind::Info, folder.path.string(),
                            gamesText(folder.apps.size(), bytes),
                            ec ? "Not mounted" : spaceText(space.available, space.capacity)));
        }
        return page;
    }
    page.rows.push_back(ui::makeRow(std::string{addId}, RowKind::Folder, "Add a Steam library",
                                    "Steam makes its steamapps folder in it",
                                    busy_ ? "Steam is working" : ""));
    for (const steam::SteamLibrary& library : *live_) {
        const std::string index = std::to_string(library.index);
        page.rows.push_back(
            ui::makeRow(std::string{libraryId} + index, RowKind::Action, library.path.string(),
                        gamesText(library.apps, library.appBytes) + " · " +
                            (library.mounted ? spaceText(library.freeSpace, library.capacity)
                                             : std::string{"not mounted"}),
                        library.isDefault ? "Default" : "Make default"));
        if (library.index != 0) {
            const std::string refused = steam::removalRefusal(library);
            page.rows.push_back(
                ui::makeRow(std::string{removeId} + index, RowKind::Action, "Remove this library",
                            refused.empty() ? library.path.string() : "Not while " + refused,
                            armed_ == library.index ? "Press again to remove" : "Remove"));
        }
    }
    return page;
}

void SteamLibrarySettings::run(const std::string& id, int step) {
    const std::optional<int> remove = indexIn(id, removeId);
    if (!remove) {
        armed_.reset();
    }
    if (step != 0) {
        return;
    }
    if (id == addId) {
        addLibrary();
        return;
    }
    std::optional<steam::SteamLibrary> library;
    {
        const std::lock_guard lock{mutex_};
        const std::optional<int> index = remove ? remove : indexIn(id, libraryId);
        if (const steam::SteamLibrary* found = index ? find(*index) : nullptr) {
            library = *found;
        }
    }
    if (!library) {
        return;
    }
    if (!remove) {
        if (!library->isDefault) {
            submit([chosen = *library](steam::Libraries& live) {
                return live.makeDefault(chosen);
            });
        }
        return;
    }
    if (const std::string refused = steam::removalRefusal(*library); !refused.empty()) {
        hooks_.say(library->path.string() + " cannot be removed: " + refused, true);
        return;
    }
    if (armed_ != library->index) {
        armed_ = library->index;
        return;
    }
    armed_.reset();
    submit([chosen = *library](steam::Libraries& live) {
        return live.remove(chosen);
    });
}

void SteamLibrarySettings::addLibrary() {
    PathEditor::Request request;
    request.title = "Add a Steam library";
    request.start = home_;
    request.refusal = [](const std::filesystem::path& folder) {
        return settings::refusal(folder);
    };
    request.done = [this](const std::optional<std::filesystem::path>& folder) {
        if (folder) {
            submit([chosen = *folder](steam::Libraries& live) {
                return live.add(chosen);
            });
        }
    };
    paths_.begin(std::move(request));
}

} // namespace opensu::app
