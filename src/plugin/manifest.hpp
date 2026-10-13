// manifest — a plugin's `plugin.json`, read without running any of its code: who it is, which host
// API it was written for, and what it may touch. Read paths are written relative to path tokens
// (`{xdg_data}/eden/play_time`), expanded against this machine's directories.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::plugin {

/// The one host API version this openSU runs.
inline constexpr int apiVersion = 1;

/// The directories path tokens stand for: `{home}`, `{xdg_data}`, `{xdg_config}`, `{xdg_cache}`,
/// and `{flatpak_data:<app id>}` and `{flatpak_cache:<app id>}` under `home`.
struct PathTokens {
    std::filesystem::path home;
    std::filesystem::path xdgData;
    std::filesystem::path xdgConfig;
    std::filesystem::path xdgCache;
};

struct Manifest {
    /// `[a-z0-9-]+`, unique among the plugins.
    std::string id;
    std::string name;
    std::string version;
    int api{0};
    /// Named groups of read paths, as written.
    std::map<std::string, std::vector<std::string>> read;
    /// The host names it may fetch from over HTTPS.
    std::vector<std::string> http;

    /// Every permission as one comparable, sorted list: `read:<path as written>`, `http:<host>`.
    [[nodiscard]] std::vector<std::string> permissions() const;
};

struct ManifestRead {
    std::optional<Manifest> manifest;
    /// Why the manifest was refused.
    std::string error;
};

/// The manifest in `text`, or why it is refused: not JSON, a field missing or of the wrong kind,
/// an id that is not `[a-z0-9-]+`, a read path with an unknown token, a relative path or `..`.
[[nodiscard]] ManifestRead parseManifest(std::string_view text);

/// `pattern` with its token replaced by the directory it stands for; nothing for an unknown token
/// or a path that is not absolute after expansion.
[[nodiscard]] std::optional<std::filesystem::path> expandPath(std::string_view pattern,
                                                              const PathTokens& tokens);

} // namespace opensu::plugin
