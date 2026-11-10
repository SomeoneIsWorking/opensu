// visual_presets — the visual preset catalog (`assets/visual-presets.json`): each preset's shader
// preset in the slang pack (with a built-in filter for when the pack is not there), its aspect, the
// consoles Auto Console gives it to, and per orientation the art framed around the picture with the
// picture's rect in it. The art is never shipped: each image downloads on demand from the URL the
// catalog pins, checked by its size and SHA-256 and kept to that URL's origin, into
// `<data>/opensu/overlays/<id>.png`. `apply` puts a preset into a game's video settings.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "library/emulator_release.hpp"
#include "net/web_client.hpp"
#include "settings/video_settings.hpp"

namespace opensu::library::roms {

/// The download job's and the Downloads page's id for the presets' art.
inline constexpr std::string_view overlayArtId = "overlay-art";
inline constexpr std::string_view overlayArtName = "Overlay art";

/// An image of the catalog, as it is downloaded.
struct PresetArt {
    std::string id;
    std::string url;
    std::uint64_t size{0};
    /// Lower-case hex.
    std::string sha256;

    bool operator==(const PresetArt&) const = default;
};

/// A preset's art for one orientation: which image, and where in its pixels the picture stands.
struct PresetLayout {
    std::string art;
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    bool operator==(const PresetLayout&) const = default;
};

struct VisualPreset {
    /// One of `settings::presetChoices` for a preset the player picks; a console's own otherwise.
    std::string id;
    /// A `.slangp` path in the shader pack; empty for none.
    std::string shader;
    /// Drawn instead when the shader is not there, or the only pass without one.
    settings::Filter filter{settings::Filter::None};
    std::optional<settings::Aspect> aspect;
    /// The consoles (`RomSystem` keys) Auto Console gives this preset to.
    std::vector<std::string> consoles;
    std::optional<PresetLayout> landscape;
    std::optional<PresetLayout> portrait;

    bool operator==(const VisualPreset&) const = default;
};

class VisualPresets {
  public:
    /// No presets: every choice runs as None.
    VisualPresets() = default;

    /// The catalog in `text`, its art kept in `folder`; nothing with `error` when it is malformed.
    [[nodiscard]] static std::optional<VisualPresets>
    parse(std::string_view text, std::filesystem::path folder, std::string& error);
    /// The catalog shipped in `assetsDir`, its art under `<dataDir>/overlays`; none, said in the
    /// log, when it cannot be read.
    [[nodiscard]] static VisualPresets shipped(const std::filesystem::path& assetsDir,
                                               const std::filesystem::path& dataDir);

    /// The preset `choice` (a `video.preset` value) stands for on `system`: Auto Console's is the
    /// console's own; null for None and for a console without one.
    [[nodiscard]] const VisualPreset* chosen(std::string_view choice,
                                             std::string_view system) const;
    [[nodiscard]] const VisualPreset* find(std::string_view id) const;
    [[nodiscard]] const VisualPreset* forConsole(std::string_view system) const;

    /// Sets `video` as `preset` has it: its shader under `shaderRoot` when the pack holds it, else
    /// its filter, its aspect, and the art of each orientation that is downloaded. False when some
    /// of its art is not.
    [[nodiscard]] bool apply(const VisualPreset& preset, settings::VideoSettings& video,
                             const std::filesystem::path& shaderRoot) const;

    [[nodiscard]] const std::vector<PresetArt>& art() const noexcept {
        return art_;
    }
    /// Where `art` is kept once downloaded.
    [[nodiscard]] std::filesystem::path fileOf(const PresetArt& art) const;
    [[nodiscard]] bool downloaded(const PresetArt& art) const;
    /// The art not downloaded yet.
    [[nodiscard]] std::vector<PresetArt> missing() const;
    /// The art that is downloaded.
    [[nodiscard]] std::size_t downloadedCount() const;
    /// What all the art comes to, in bytes.
    [[nodiscard]] std::uint64_t totalSize() const;
    /// The folder art is kept in; empty for none.
    [[nodiscard]] const std::filesystem::path& folder() const noexcept {
        return folder_;
    }

    /// Downloads the art that is missing through `release`, each checked by size and SHA-256,
    /// none fetched from another origin than its URL's. Unless one succeeds, nothing of it stays.
    /// The count downloaded.
    [[nodiscard]] net::Result<std::size_t> install(const std::stop_token& stop,
                                                   const EmulatorRelease& release,
                                                   const net::WebClient::Progress& progress) const;
    /// Removes the downloaded art. False with `error` when some stays.
    [[nodiscard]] bool remove(std::string& error) const;

  private:
    [[nodiscard]] const PresetArt* artNamed(std::string_view id) const;
    /// `layout` as frame art, or nothing when its image is not downloaded.
    [[nodiscard]] std::optional<settings::FrameArt> frameOf(const PresetLayout& layout) const;

    std::vector<PresetArt> art_;
    std::vector<VisualPreset> presets_;
    std::filesystem::path folder_;
};

} // namespace opensu::library::roms
