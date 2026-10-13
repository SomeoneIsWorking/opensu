// zip_archive — reading zip files from bytes: the end record, the central directory, a local
// header's data offset and one entry's inflated, checked contents. No I/O; the caller fetches
// the bytes. zip64 is refused.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::artwork::zip {

/// An entry's central directory record.
struct Entry {
    std::string name;
    /// 0 stored, 8 deflate.
    std::uint16_t method{0};
    std::uint32_t crc32{0};
    std::uint32_t compressedSize{0};
    std::uint32_t size{0};
    std::uint32_t localHeaderOffset{0};
};

/// Where the central directory is, from the end record.
struct Directory {
    std::uint32_t offset{0};
    std::uint32_t size{0};
    std::uint16_t entries{0};
};

/// The fixed part of a local header, which holds the lengths of its name and extra field.
inline constexpr std::size_t localHeaderSize = 30;

/// The end record in `tail`, the last bytes of a zip that starts at `tailOffset` in the file.
/// Nothing, with `error`, when there is none or the zip is zip64 or split.
[[nodiscard]] std::optional<Directory> findDirectory(std::string_view tail,
                                                     std::uint64_t tailOffset, std::string& error);

/// The entries of a central directory of `directory.entries` records.
[[nodiscard]] std::optional<std::vector<Entry>>
parseDirectory(std::string_view bytes, const Directory& directory, std::string& error);

/// How far into the file an entry's data starts, from the first `localHeaderSize` bytes of its
/// local header.
[[nodiscard]] std::optional<std::uint64_t> dataOffset(const Entry& entry, std::string_view header,
                                                      std::string& error);

/// An entry's data (as stored in the zip) turned back into its file, its size and CRC-32 checked.
[[nodiscard]] std::optional<std::string> extract(const Entry& entry, std::string_view data,
                                                 std::string& error);

/// A whole zip held in memory.
class Archive {
  public:
    [[nodiscard]] static std::optional<Archive> open(std::string bytes, std::string& error);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept {
        return entries_;
    }
    /// The named entry's file, or nothing: `error` is empty when the zip has no such entry and
    /// says why otherwise.
    [[nodiscard]] std::optional<std::string> read(std::string_view name, std::string& error) const;

  private:
    Archive(std::string bytes, std::vector<Entry> entries)
        : bytes_{std::move(bytes)}, entries_{std::move(entries)} {
    }

    std::string bytes_;
    std::vector<Entry> entries_;
};

} // namespace opensu::artwork::zip
