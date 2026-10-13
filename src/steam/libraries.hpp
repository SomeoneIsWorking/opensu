// libraries — Steam's library folders as the running client keeps them, through the
// `SteamClient.InstallFolder` API of its SharedJSContext (the calls Steam's own Storage page
// makes): list them, add one, make one the default, remove an empty one.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "devtools.hpp"

namespace opensu::steam {

struct SteamLibrary {
    /// Steam's index, which the calls name the library by.
    int index{0};
    std::filesystem::path path;
    std::string label;
    std::uint64_t capacity{0};
    std::uint64_t freeSpace{0};
    /// The installed apps and their bytes on disk.
    std::size_t apps{0};
    std::uint64_t appBytes{0};
    bool isDefault{false};
    bool mounted{true};

    bool operator==(const SteamLibrary&) const = default;
};

/// The libraries in `GetInstallFolders()`'s answer.
[[nodiscard]] std::vector<SteamLibrary> librariesOf(const nlohmann::json& folders);

/// Why Steam would not remove `library`, as Steam's own page decides it; empty when it would.
[[nodiscard]] std::string removalRefusal(const SteamLibrary& library);

class Libraries {
  public:
    explicit Libraries(DevTools& devTools) : devTools_{devTools} {
    }

    /// Every library; nothing, with `error`, when Steam cannot be asked.
    [[nodiscard]] std::optional<std::vector<SteamLibrary>> read(std::string& error);
    /// Adds `folder` as a library. Each returns empty on success, else why not.
    [[nodiscard]] std::string add(const std::filesystem::path& folder);
    [[nodiscard]] std::string makeDefault(const SteamLibrary& library);
    /// Refused, with the reason, while the library holds games or is Steam's own.
    [[nodiscard]] std::string remove(const SteamLibrary& library);

  private:
    DevTools& devTools_;
};

} // namespace opensu::steam
