// gog_builds — the build GOG offers now for a game, read where gogdl reads it, so an install can be
// compared with it. Windows builds come from the content system
// (`content-system.gog.com/products/<id>/os/windows/builds?generation=2`,
// `gogdl/dl/managers/manager.py:get_builds`); gogdl installs Linux games from GOG's installers
// instead (`api.gog.com/products/<id>?expand=downloads`, `gogdl/dl/managers/linux.py:setup`), so a
// Linux install is compared by installer version.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gog_auth.hpp"
#include "net/web_client.hpp"

namespace opensu::library::gog {

/// The build gogdl downloads from a content-system builds answer: the first on no branch, else the
/// first. Nothing when there is none. Throws std::runtime_error for an answer of another shape.
[[nodiscard]] std::optional<std::string> offeredWindowsBuild(std::string_view body);

/// The version of the Linux installer gogdl downloads from a product answer with its downloads:
/// the only Linux installer, else the English one. Nothing when there is none or it has no version.
/// Throws std::runtime_error for an answer of another shape.
[[nodiscard]] std::optional<std::string> offeredLinuxVersion(std::string_view body);

/// What GOG offers now for game `id` on gogdl platform `platform` ("windows" or "linux"), in the
/// terms `installedBuild` reads an install in; `headers` carry the sign-in. Throws
/// std::runtime_error when GOG cannot be read.
[[nodiscard]] std::optional<std::string> offeredBuild(const net::WebClient& web,
                                                      const Endpoints& endpoints,
                                                      const std::vector<std::string>& headers,
                                                      std::string_view id,
                                                      std::string_view platform);

/// Whether an install of build `installed` is behind `offered`; an unknown side is no update.
[[nodiscard]] bool behind(std::string_view installed, const std::optional<std::string>& offered);

} // namespace opensu::library::gog
