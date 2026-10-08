#include "settings.hpp"

#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace iideck::settings {
namespace {

using json = nlohmann::json;

constexpr const char* libraryModeKey = "libraryMode";

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
    const json document{{libraryModeKey, std::string{library::key(settings.libraryMode)}}};
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace iideck::settings
