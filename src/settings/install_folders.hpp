// install_folders — where games install: one default folder and an optional folder per store. The
// one resolver every store's install flow reads its destination from.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "library/game.hpp"

namespace opensu::settings {

/// The stores opensu installs from.
inline constexpr std::array installStores{library::Source::Steam, library::Source::Epic,
                                          library::Source::Gog};

class InstallFolders {
  public:
    /// The folder every store installs into unless it has its own; empty for none.
    [[nodiscard]] const std::filesystem::path& defaultFolder() const noexcept {
        return default_;
    }
    void setDefault(std::filesystem::path folder) {
        default_ = std::move(folder);
    }

    /// The store's own folder; empty when it follows the default.
    [[nodiscard]] std::filesystem::path storeFolder(library::Source store) const;
    /// Sets the store's own folder; an empty `folder` makes the store follow the default again.
    void setStore(library::Source store, std::filesystem::path folder);

    /// Where `store` installs: its own folder, else the default, else nothing, which leaves the
    /// store's own choice.
    [[nodiscard]] std::optional<std::filesystem::path> effective(library::Source store) const;

    bool operator==(const InstallFolders&) const = default;

  private:
    std::filesystem::path default_;
    std::map<library::Source, std::filesystem::path> stores_;
};

/// What a folder must allow.
enum class Need : std::uint8_t { Read, Write };

/// Why `folder` cannot be used: it is not absolute, does not exist, is not a folder or cannot be
/// read (or, for `Write`, written). Empty when it can.
[[nodiscard]] std::string refusal(const std::filesystem::path& folder, Need need = Need::Write);

} // namespace opensu::settings
