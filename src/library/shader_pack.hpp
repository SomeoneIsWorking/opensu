// shader_pack — libretro's slang shader pack, which the player downloads for shader presets: the
// buildbot's `shaders_slang.zip`, unpacked whole under `<data>/opensu/shaders/slang/` with a
// record beside it (`slang.json`: Last-Modified, size, URL) so a newer pack can be told apart, its
// update check, its removal, and the `.slangp` presets it holds by top-level folder. The buildbot
// publishes no digest for the zip: a download comes over HTTPS from the buildbot's origin only,
// must be as long as its Content-Length says, and every file in it is checked against its zip
// CRC-32 as it is unpacked. The pack's shaders carry their own licences; openSU never ships them.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "net/web_client.hpp"

namespace opensu::library::roms {

inline constexpr std::string_view shaderPackUrl =
    "https://buildbot.libretro.com/assets/frontend/shaders_slang.zip";
/// The download job's and the Downloads page's id for the pack.
inline constexpr std::string_view shaderPackId = "shader-presets";
inline constexpr std::string_view shaderPackName = "Shader presets";
inline constexpr std::string_view presetExtension = ".slangp";

/// One build of the pack as the buildbot serves it.
struct ShaderPackBuild {
    /// Last-Modified, as Unix time.
    std::int64_t modified{0};
    std::uint64_t size{0};
    std::string url;

    /// "2026-10-21 14:05" (UTC): the build's version as the player reads it.
    [[nodiscard]] std::string version() const;
    /// What tells two builds apart.
    [[nodiscard]] std::string key() const;

    bool operator==(const ShaderPackBuild&) const = default;
};

/// A preset in the pack.
struct ShaderPreset {
    /// Its folder in the pack ("crt", "bezel/Mega_Bezel/Presets"); empty for one at the top.
    std::string folder;
    /// Its path in the pack, as the `video.shader` setting keeps it ("crt/crt-royale.slangp").
    std::string path;
    /// Its file name without the extension ("crt-royale").
    std::string name;

    bool operator==(const ShaderPreset&) const = default;
};

/// The name of the preset at `path`: its file name without the extension.
[[nodiscard]] std::string presetName(std::string_view path);

/// The pack on disk: `<folder>/slang/` and its record `<folder>/slang.json`.
class ShaderPack {
  public:
    explicit ShaderPack(std::filesystem::path folder) : folder_{std::move(folder)} {
    }
    /// The pack under openSU's data directory: `<dataDir>/shaders`.
    [[nodiscard]] static ShaderPack under(const std::filesystem::path& dataDir);

    /// Where the presets are; the `video.shader` setting is relative to it.
    [[nodiscard]] std::filesystem::path root() const;

    /// The installed build, or nothing when none is (or its folder is gone).
    [[nodiscard]] std::optional<ShaderPackBuild> installed() const;

    /// Unpacks the zip `bytes` of `build` beside the pack, checking every file, and switches to
    /// it. False with `error` when it cannot; the pack before stays then.
    [[nodiscard]] bool install(const ShaderPackBuild& build, std::string bytes,
                               std::string& error) const;

    /// Removes the pack and its record. False with `error` when something stays behind.
    [[nodiscard]] bool remove(std::string& error) const;

    /// Every preset in the pack, by folder (the top's first) then path.
    [[nodiscard]] std::vector<ShaderPreset> presets() const;

  private:
    [[nodiscard]] std::filesystem::path record() const;

    std::filesystem::path folder_;
};

class ShaderPackDownload {
  public:
    ShaderPackDownload(const net::WebClient& web, std::string url)
        : web_{web}, url_{std::move(url)} {
    }

    /// The build the buildbot serves now, from a HEAD of the zip.
    [[nodiscard]] std::optional<ShaderPackBuild> latest(std::string& error) const;

    /// Downloads the zip, checks it and unpacks it into `pack`. On any failure, a cancel included,
    /// nothing of the download stays, the pack in use stays, and `error` says why.
    [[nodiscard]] std::optional<ShaderPackBuild> install(const ShaderPack& pack,
                                                         const net::WebClient::Progress& progress,
                                                         std::string& error) const;

  private:
    /// Why `url` may not be fetched: every hop stays on the buildbot's origin.
    [[nodiscard]] std::string refusal(const std::string& url) const;

    const net::WebClient& web_;
    std::string url_;
};

} // namespace opensu::library::roms
