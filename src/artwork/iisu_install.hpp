// iisu_install — iiSU's file as the player installs it: iiSU's release APK or starter pack placed
// in openSU's iiSU folder (`<data dir>/themes/iiSU/`). The folder is read when asked and the file
// opened again only when what is there changed; a file there that is not iiSU's is logged and
// passed over. With none, iiSU's art and sounds are not offered.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "iisu_pack.hpp"

namespace opensu::artwork {

/// The folder under the player's themes folder that iiSU's file is placed in.
inline constexpr std::string_view iisuFolderName = "iiSU";

class IisuInstall {
  public:
    explicit IisuInstall(std::filesystem::path folder) : folder_{std::move(folder)} {
    }

    [[nodiscard]] const std::filesystem::path& folder() const noexcept {
        return folder_;
    }

    /// iiSU's file in the folder, the release APK (art and sounds) before a starter pack (art
    /// only); nothing when there is none. The same pack while the folder is unchanged.
    [[nodiscard]] std::shared_ptr<const IisuPack> pack() const;

  private:
    /// An APK or zip in the folder, as it was when read.
    struct Placed {
        std::filesystem::path file;
        std::uintmax_t size{0};
        std::filesystem::file_time_type written;
        bool operator==(const Placed&) const = default;
    };

    [[nodiscard]] std::vector<Placed> placed() const;

    std::filesystem::path folder_;
    mutable std::mutex mutex_;
    mutable std::optional<std::vector<Placed>> read_;
    mutable std::shared_ptr<const IisuPack> pack_;
};

} // namespace opensu::artwork
