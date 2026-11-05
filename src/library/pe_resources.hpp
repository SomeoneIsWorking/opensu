// pe_resources — the resources of a Windows program, read portably from the file: the resource
// directory's type, name and language levels. A program is an untrusted file, so every offset is
// checked against the section read; one that points outside it reads as nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace opensu::library::pe {

/// PE's resource types.
inline constexpr std::uint32_t iconType = 3;
inline constexpr std::uint32_t iconGroupType = 14;
inline constexpr std::uint32_t versionType = 16;

class Resources {
  public:
    /// The resources of `file`; nothing when it is no PE file or has none.
    [[nodiscard]] static std::optional<Resources> of(const std::filesystem::path& file);

    /// The resource section whose first byte is at `rva` in the image.
    Resources(std::vector<std::uint8_t> section, std::uint32_t rva)
        : section_{std::move(section)}, rva_{rva} {
    }

    /// The ids of `type`'s resources in directory order, 0 for a named one, which Windows lists
    /// first.
    [[nodiscard]] std::vector<std::uint32_t> ids(std::uint32_t type) const;
    /// Resource `index` of `type`, in its first language; empty when there is none.
    [[nodiscard]] std::span<const std::uint8_t> at(std::uint32_t type, std::size_t index) const;
    /// Resource `id` of `type`, in its first language; empty when there is none.
    [[nodiscard]] std::span<const std::uint8_t> byId(std::uint32_t type, std::uint32_t id) const;

  private:
    struct Entry {
        std::uint32_t id;
        bool named;
        /// The offset of what it points at, its high bit set for a directory.
        std::uint32_t target;
    };
    [[nodiscard]] std::vector<Entry> directory(std::uint32_t offset) const;
    /// The entries under `type`.
    [[nodiscard]] std::vector<Entry> ofType(std::uint32_t type) const;
    /// The data at `entry`, a name's: its first language's.
    [[nodiscard]] std::span<const std::uint8_t> data(const Entry& entry) const;

    std::vector<std::uint8_t> section_;
    std::uint32_t rva_;
};

} // namespace opensu::library::pe
