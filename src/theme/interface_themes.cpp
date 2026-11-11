#include "interface_themes.hpp"

#include <iterator>
#include <optional>
#include <utility>

#include "lucent/log.h"

namespace opensu::theme {

std::vector<std::shared_ptr<const InterfaceTheme>> InterfaceThemes::list() const {
    std::vector<std::shared_ptr<const InterfaceTheme>> themes;
    std::vector<ThemeFolder> folders = folders_.shipped();
    std::vector<ThemeFolder> own = folders_.user();
    folders.insert(folders.end(), std::make_move_iterator(own.begin()),
                   std::make_move_iterator(own.end()));
    for (ThemeFolder& folder : folders) {
        std::string error;
        if (std::optional<InterfaceTheme> theme =
                readTheme(folder.folder, std::move(folder.id), error)) {
            themes.push_back(std::make_shared<const InterfaceTheme>(std::move(*theme)));
        }
    }
    return themes;
}

std::shared_ptr<const InterfaceTheme> InterfaceThemes::open(std::string_view id,
                                                            std::string& error) const {
    const std::optional<std::filesystem::path> folder = folders_.folderOf(id, error);
    if (!folder) {
        return nullptr;
    }
    std::optional<InterfaceTheme> theme =
        readTheme(*folder, std::string{id.empty() ? defaultInterfaceTheme : id}, error);
    return theme ? std::make_shared<const InterfaceTheme>(std::move(*theme)) : nullptr;
}

std::shared_ptr<const InterfaceTheme> InterfaceThemes::openOrDefault(std::string_view id) const {
    std::string error;
    if (std::shared_ptr<const InterfaceTheme> theme = open(id, error)) {
        lucent::info("theme", "interface theme {} from {}", theme->name, theme->folder.string());
        return theme;
    }
    lucent::warn("theme", "interface theme {}: {}; showing {}", id, error, defaultInterfaceTheme);
    if (std::shared_ptr<const InterfaceTheme> theme = open({}, error)) {
        return theme;
    }
    lucent::error("theme", "the default interface theme: {}; drawing the tokens' defaults", error);
    InterfaceTheme fallback;
    fallback.id = std::string{defaultInterfaceTheme};
    fallback.name = "iiSU";
    return std::make_shared<const InterfaceTheme>(std::move(fallback));
}

} // namespace opensu::theme
