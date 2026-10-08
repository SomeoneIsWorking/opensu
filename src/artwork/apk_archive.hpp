// apk_archive — iiSU's release APK read by HTTP ranges: the end record and central directory
// once, then any entry's header and data on demand, never the whole APK.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "net/web_client.hpp"
#include "zip_archive.hpp"

namespace iideck::artwork {

class ApkArchive {
  public:
    /// The APK at `url`, which is `size` bytes long. Nothing is fetched until the first use.
    ApkArchive(std::string url, std::uint64_t size);

    /// The named entry's record, reading the directory on first use. Nothing when the APK has no
    /// such entry (`error` empty) or cannot be read (`error` says why).
    [[nodiscard]] const zip::Entry* find(const net::WebClient& web, std::string_view name,
                                         std::string& error);

    /// The entry's file, inflated and checked against its CRC-32.
    [[nodiscard]] std::optional<std::string> extract(const net::WebClient& web,
                                                     const zip::Entry& entry, std::string& error);

  private:
    [[nodiscard]] bool load(const net::WebClient& web, std::string& error);
    [[nodiscard]] std::optional<std::string> span(const net::WebClient& web, std::uint64_t first,
                                                  std::uint64_t last, std::string& error) const;

    std::string url_;
    std::uint64_t size_;
    std::optional<std::vector<zip::Entry>> entries_;
};

} // namespace iideck::artwork
