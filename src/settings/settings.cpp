#include "settings.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::settings {
namespace {

using json = nlohmann::json;

constexpr const char* libraryModeKey = "libraryMode";
constexpr const char* pinLibraryDockKey = "pinLibraryDock";
constexpr const char* iconSizeKey = "iconSize";
constexpr const char* sortKey = "sort";
constexpr const char* installedOnlyKey = "installedOnly";
constexpr const char* sourceKey = "source";
constexpr const char* hiddenKey = "hidden";
constexpr const char* lastPlayedKey = "lastPlayed";

/// Reads a file's fields, each on its own: one that is damaged is reported and keeps its default.
class Reader {
  public:
    Reader(const json& document, std::string file) : document_{document}, file_{std::move(file)} {
    }

    void boolean(const char* name, bool& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_boolean()) {
            out = found->get<bool>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void integer(const char* name, int low, int high, int& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_number_integer() && found->get<long long>() >= low &&
            found->get<long long>() <= high) {
            out = found->get<int>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void text(const char* name, std::string& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_string()) {
            out = found->get<std::string>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void texts(const char* name, std::vector<std::string>& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        const bool strings = found->is_array() && std::ranges::all_of(*found, [](const json& item) {
                                 return item.is_string();
                             });
        if (strings) {
            out = found->get<std::vector<std::string>>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void times(const char* name, library::PlayHistory::Entries& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        bool numbers = found->is_object();
        for (const auto& item : found->items()) {
            numbers = numbers && item.value().is_number_integer();
        }
        if (numbers) {
            out = found->get<library::PlayHistory::Entries>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void reject(const char* name, const json& value, const char* fallback) const {
        lucent::warn("settings", "{}: {} is not a valid {}; {}", file_, value.dump(), name,
                     fallback);
    }

  private:
    const json& document_;
    std::string file_;
};

} // namespace

Store::Store(std::filesystem::path file) : file_{std::move(file)} {
}

Settings Store::load() const {
    Settings settings;
    std::ifstream in{file_, std::ios::binary};
    if (!in) {
        return settings;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        lucent::warn("settings", "{} is not a settings file; using the defaults", file_.string());
        return settings;
    }
    const Reader read{document, file_.string()};
    read.boolean(pinLibraryDockKey, settings.pinLibraryDock, "the dock stays pinned");
    read.integer(iconSizeKey, minIconSize, maxIconSize, settings.iconSize, "using the default");
    read.boolean(installedOnlyKey, settings.view.installedOnly, "showing every game");
    read.text(sourceKey, settings.view.source, "showing every source");
    std::vector<std::string> hidden;
    read.texts(hiddenKey, hidden, "no game is hidden");
    settings.hidden = library::HiddenGames{std::move(hidden)};
    library::PlayHistory::Entries played;
    read.times(lastPlayedKey, played, "no launch is remembered");
    settings.lastPlayed = library::PlayHistory{std::move(played)};

    std::string spelling;
    read.text(sortKey, spelling, "using the default");
    if (!spelling.empty()) {
        if (const std::optional<library::SortKey> sort = library::sortKeyOf(spelling)) {
            settings.view.sort = *sort;
        } else {
            lucent::warn("settings", "{}: {} is not a sort; using the default", file_.string(),
                         spelling);
        }
    }
    const auto mode = document.find(libraryModeKey);
    if (mode == document.end()) {
        return settings;
    }
    const std::optional<library::LibraryMode> chosen =
        mode->is_string() ? library::libraryModeOf(mode->get<std::string>()) : std::nullopt;
    if (chosen) {
        settings.libraryMode = *chosen;
    } else {
        lucent::warn("settings", "{}: {} is not a Library mode; using Standard", file_.string(),
                     mode->dump());
    }
    return settings;
}

bool Store::save(const Settings& settings, std::string& error) const {
    const json document{{libraryModeKey, std::string{library::key(settings.libraryMode)}},
                        {pinLibraryDockKey, settings.pinLibraryDock},
                        {iconSizeKey, settings.iconSize},
                        {sortKey, std::string{library::key(settings.view.sort)}},
                        {installedOnlyKey, settings.view.installedOnly},
                        {sourceKey, settings.view.source},
                        {hiddenKey, settings.hidden.keys()},
                        {lastPlayedKey, settings.lastPlayed.entries()}};
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace opensu::settings
