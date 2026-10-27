// shader_preset_picker — the "Shader preset" picker of a setting layer's page: the downloaded pack
// as its folder tree (crt, handheld, bezel › Mega_Bezel › ...), each level its subfolders then its
// presets, with "Not set" (the layer below decides) and "None" (no preset, over any layer below)
// first at the top. The pack is listed once as the picker opens. What a press picks goes back to
// `LayerSettings`, which keeps it.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "library/shader_pack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class ShaderPresetPicker {
  public:
    explicit ShaderPresetPicker(const library::roms::ShaderPack& pack) : pack_{pack} {
    }

    /// What a press picked.
    struct Pick {
        enum class Kind : std::uint8_t {
            /// Moved within the picker.
            Nothing,
            /// The layer keeps no preset of its own.
            Clear,
            /// The layer keeps `value` ("" for None).
            Set,
        };
        Kind kind{Kind::Nothing};
        std::string value;
    };

    /// Opens at the pack's top, listing the pack.
    void open();
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }
    /// The pack is downloaded.
    [[nodiscard]] bool packInstalled() const {
        return pack_.installed().has_value();
    }
    /// "Shader preset", then the open folder's path.
    [[nodiscard]] std::string openName() const;

    /// The open folder's rows; `own` is the layer's value (null when it keeps none) and
    /// `inherited` what applies without it.
    [[nodiscard]] std::vector<ui::SettingsRow> rows(const std::string* own,
                                                    const std::string& inherited) const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`.
    [[nodiscard]] Pick press(const std::string& id);
    /// Back one level: from a folder to its parent (true), or out of the picker (false).
    bool back();
    /// The row to focus after the last move: the folder just left; empty for the first row.
    [[nodiscard]] const std::string& returnRow() const noexcept {
        return returnRow_;
    }

  private:
    const library::roms::ShaderPack& pack_;
    std::vector<library::roms::ShaderPreset> presets_;
    /// The open folder's path in the pack, "" for its top.
    std::string folder_;
    std::string returnRow_;
    bool open_{false};
};

/// A preset value as a row reads it: "None" for "", else its folders and name ("crt ›
/// crt-royale").
[[nodiscard]] std::string presetLabel(std::string_view value);

} // namespace opensu::app
