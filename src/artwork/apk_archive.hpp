// apk_archive — a zip read by byte ranges: the end record and central directory once, then any
// entry's header and data on demand, never the whole file. iiSU's release APK is read this way
// over HTTP for its UI sounds, and the iiSU file the player gives openSU from disk.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "net/web_client.hpp"
#include "zip/zip_archive.hpp"

namespace opensu::artwork {

inline constexpr std::string_view iisuApkUrl =
    "https://github.com/iisu-network/iiSU/releases/download/0.0.7.4/iiSU-Alpha-7.4.apk";
/// The pinned release's size, which its ranged reads are checked against.
inline constexpr std::uint64_t iisuApkBytes = 128322093;

/// What looking for a file in the APK came to.
struct ApkFile {
    enum class Status : std::uint8_t {
        Found,
        /// The APK has no such file.
        Missing,
        /// The APK cannot be read or the entry fails its checks; `error` says why.
        Failed,
    };

    Status status{Status::Missing};
    /// The file, inflated, when found.
    std::string bytes;
    std::string error;
};

class ApkArchive {
  public:
    /// Reads bytes `first` to `last` of the zip, both included; nothing, with `error`, when it
    /// cannot.
    using RangeRead = std::function<std::optional<std::string>(
        std::uint64_t first, std::uint64_t last, std::string& error)>;

    /// The zip `size` bytes long that `read` reads. Nothing is read until the first use.
    ApkArchive(std::uint64_t size, RangeRead read);

    /// The APK at `url` through `web`'s HTTP ranges; `web` outlives the archive.
    [[nodiscard]] static ApkArchive overHttp(const net::WebClient& web, std::string url,
                                             std::uint64_t size);
    /// The zip in `file` on disk. Nothing, with `error`, when it cannot be opened.
    [[nodiscard]] static std::optional<ApkArchive> overFile(const std::filesystem::path& file,
                                                            std::string& error);

    /// Every entry's record, reading the directory on first use; null with `error` when the zip
    /// cannot be read.
    [[nodiscard]] const std::vector<zip::Entry>* entries(std::string& error);

    /// The named entry's record. Nothing when the zip has no such entry (`error` empty) or cannot
    /// be read (`error` says why).
    [[nodiscard]] const zip::Entry* find(std::string_view name, std::string& error);

    /// The entry's file, inflated and checked against its CRC-32; one over `maxSize` is refused.
    [[nodiscard]] std::optional<std::string> extract(const zip::Entry& entry, std::string& error,
                                                     std::uint32_t maxSize = zip::maxFileSize);

    /// The named file, inflated and checked: find and extract in one.
    [[nodiscard]] ApkFile fetch(std::string_view name);

  private:
    [[nodiscard]] std::optional<std::string> span(std::uint64_t first, std::uint64_t last,
                                                  std::string& error) const;

    std::uint64_t size_;
    RangeRead read_;
    std::optional<std::vector<zip::Entry>> entries_;
};

} // namespace opensu::artwork
