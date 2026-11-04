// save_share_kind — where the player shares saves beside the LAN (Settings › Saves): nowhere, a
// folder or a WebDAV share, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::settings {

enum class SaveShareKind : std::uint8_t { Off, Folder, WebDav };

inline constexpr std::array allSaveShareKinds{SaveShareKind::Off, SaveShareKind::Folder,
                                              SaveShareKind::WebDav};

/// As the settings file spells it: "off", "folder", "webdav".
[[nodiscard]] std::string_view key(SaveShareKind kind) noexcept;
/// The kind a spelling names, or nothing.
[[nodiscard]] std::optional<SaveShareKind> saveShareKindOf(std::string_view spelling) noexcept;
/// As the player reads it: "Off", "A folder", "WebDAV".
[[nodiscard]] std::string_view label(SaveShareKind kind) noexcept;

} // namespace opensu::settings
