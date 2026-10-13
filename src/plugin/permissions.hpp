// permissions — what one plugin may touch: files under the read paths its manifest declares, and
// HTTPS to the hosts it declares. Paths are compared after resolving `..` and symlinks, so neither
// climbs out of a root.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "manifest.hpp"

namespace opensu::plugin {

class Permissions {
  public:
    Permissions() = default;
    /// The manifest's read paths expanded against `tokens`, and its hosts.
    Permissions(const Manifest& manifest, const PathTokens& tokens);

    /// Each read group's paths that exist now, resolved.
    [[nodiscard]] std::map<std::string, std::vector<std::filesystem::path>> existingRoots() const;

    /// `path` resolved, when it lies in a read path; else nothing, with `error` saying why.
    [[nodiscard]] std::optional<std::filesystem::path> readable(const std::filesystem::path& path,
                                                                std::string& error) const;

    /// Why `url` may not be fetched (not HTTPS, credentials in it, an undeclared host), or empty.
    [[nodiscard]] std::string urlRefusal(std::string_view url) const;

    /// Every expanded read path, existing or not, for the player to read.
    [[nodiscard]] std::vector<std::filesystem::path> readPaths() const;
    [[nodiscard]] const std::vector<std::string>& hosts() const noexcept {
        return hosts_;
    }

  private:
    std::map<std::string, std::vector<std::filesystem::path>> roots_;
    std::vector<std::string> hosts_;
};

} // namespace opensu::plugin
