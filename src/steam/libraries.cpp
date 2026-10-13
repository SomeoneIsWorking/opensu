#include "libraries.hpp"

#include <cstdlib>

#include "lucent/log.h"

namespace opensu::steam {
namespace {

using json = nlohmann::json;

std::uint64_t bytes(const json& object, const char* key) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return 0;
    }
    if (found->is_number_unsigned() || found->is_number_integer()) {
        return found->get<std::uint64_t>();
    }
    // Steam sends 64-bit sizes as strings.
    if (found->is_string()) {
        return std::strtoull(found->get<std::string>().c_str(), nullptr, 10);
    }
    return 0;
}

std::string textOf(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

bool flagOf(const json& object, const char* key, bool fallback) {
    const auto found = object.find(key);
    return found != object.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

} // namespace

std::vector<SteamLibrary> librariesOf(const json& folders) {
    std::vector<SteamLibrary> libraries;
    if (!folders.is_array()) {
        return libraries;
    }
    for (const json& folder : folders) {
        if (!folder.is_object()) {
            continue;
        }
        SteamLibrary library;
        const auto index = folder.find("nFolderIndex");
        library.index = index != folder.end() && index->is_number_integer() ? index->get<int>() : 0;
        library.path = textOf(folder, "strFolderPath");
        library.label = textOf(folder, "strUserLabel");
        library.capacity = bytes(folder, "nCapacity");
        library.freeSpace = bytes(folder, "nFreeSpace");
        library.isDefault = flagOf(folder, "bIsDefaultFolder", false);
        library.mounted = flagOf(folder, "bIsMounted", true);
        if (const auto apps = folder.find("vecApps"); apps != folder.end() && apps->is_array()) {
            library.apps = apps->size();
            for (const json& app : *apps) {
                library.appBytes += app.is_object() ? bytes(app, "nAppSize") : 0;
            }
        }
        libraries.push_back(std::move(library));
    }
    return libraries;
}

std::string removalRefusal(const SteamLibrary& library) {
    if (library.index == 0) {
        return "it is Steam's own library";
    }
    if (library.apps > 0) {
        return "it still holds " + std::to_string(library.apps) +
               (library.apps == 1 ? " game" : " games") + "; uninstall or move them in Steam first";
    }
    return {};
}

std::optional<std::vector<SteamLibrary>> Libraries::read(std::string& error) {
    const std::optional<json> folders =
        devTools_.evaluate("SteamClient.InstallFolder.GetInstallFolders()", error);
    if (!folders || !folders->is_array()) {
        error = error.empty() ? "Steam did not list its libraries" : error;
        return std::nullopt;
    }
    return librariesOf(*folders);
}

std::string Libraries::add(const std::filesystem::path& folder) {
    std::string error;
    if (!devTools_.evaluate("SteamClient.InstallFolder.AddInstallFolder(" +
                                json(folder.string()).dump() + ")",
                            error)) {
        return "Steam would not add " + folder.string() + ": " + error;
    }
    lucent::info("steam", "added the library {}", folder.string());
    return {};
}

std::string Libraries::makeDefault(const SteamLibrary& library) {
    std::string error;
    if (!devTools_.evaluate("SteamClient.InstallFolder.SetDefaultInstallFolder(" +
                                std::to_string(library.index) + ")",
                            error)) {
        return "Steam would not make " + library.path.string() + " the default: " + error;
    }
    lucent::info("steam", "the default library is {}", library.path.string());
    return {};
}

std::string Libraries::remove(const SteamLibrary& library) {
    if (const std::string refused = removalRefusal(library); !refused.empty()) {
        return library.path.string() + " cannot be removed: " + refused;
    }
    std::string error;
    if (!devTools_.evaluate("SteamClient.InstallFolder.RemoveInstallFolder(" +
                                std::to_string(library.index) + ")",
                            error)) {
        return "Steam would not remove " + library.path.string() + ": " + error;
    }
    lucent::info("steam", "removed the library {}", library.path.string());
    return {};
}

} // namespace opensu::steam
