// folder_share — a folder the player picked (a synced or mounted one) as a `SaveShare`: files
// under it, each replaced atomically.
#pragma once

#include <filesystem>

#include "save_share.hpp"

namespace opensu::save_sync {

class FolderShare final : public SaveShare {
  public:
    explicit FolderShare(std::filesystem::path root);

    [[nodiscard]] std::string where() const override;
    [[nodiscard]] bool reachable() const override;
    [[nodiscard]] Read read(const std::string& name) const override;
    [[nodiscard]] bool write(const std::string& name, const std::string& bytes,
                             std::string& error) const override;

  private:
    std::filesystem::path root_;
};

} // namespace opensu::save_sync
