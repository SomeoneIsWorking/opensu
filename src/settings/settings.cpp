#include "settings.hpp"

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
    if (const auto pin = document.find(pinLibraryDockKey); pin != document.end()) {
        if (pin->is_boolean()) {
            settings.pinLibraryDock = pin->get<bool>();
        } else {
            lucent::warn("settings", "{}: {} is not true or false; the dock stays pinned",
                         file_.string(), pin->dump());
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
                         {pinLibraryDockKey, settings.pinLibraryDock}};
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace opensu::settings
