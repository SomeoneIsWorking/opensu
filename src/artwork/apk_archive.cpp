#include "apk_archive.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace opensu::artwork {
namespace {

// The zip's last bytes, which hold its end record.
constexpr std::uint64_t tailBytes = 64ULL * 1024;
// Each range stays small enough to finish inside the web client's timeout.
constexpr std::uint64_t chunkBytes = 4ULL * 1024 * 1024;

} // namespace

ApkArchive::ApkArchive(std::uint64_t size, RangeRead read) : size_{size}, read_{std::move(read)} {
}

std::optional<ApkArchive> ApkArchive::overFile(const std::filesystem::path& file,
                                               std::string& error) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) {
        error = "cannot read " + file.filename().string() + ": " + ec.message();
        return std::nullopt;
    }
    return ApkArchive{size,
                      [file = file](std::uint64_t first, std::uint64_t last,
                                    std::string& reason) -> std::optional<std::string> {
                          std::ifstream in{file, std::ios::binary};
                          std::string bytes(static_cast<std::size_t>(last - first + 1), '\0');
                          in.seekg(static_cast<std::streamoff>(first));
                          in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                          if (!in) {
                              reason = "cannot read " + file.filename().string();
                              return std::nullopt;
                          }
                          return bytes;
                      }};
}

std::optional<std::string> ApkArchive::span(std::uint64_t first, std::uint64_t last,
                                            std::string& error) const {
    std::string out;
    for (std::uint64_t at = first; at <= last; at += chunkBytes) {
        const std::optional<std::string> part =
            read_(at, std::min(at + chunkBytes - 1, last), error);
        if (!part) {
            return std::nullopt;
        }
        out += *part;
    }
    return out;
}

const std::vector<zip::Entry>* ApkArchive::entries(std::string& error) {
    if (entries_) {
        return &*entries_;
    }
    const std::uint64_t tailOffset = size_ > tailBytes ? size_ - tailBytes : 0;
    if (size_ == 0) {
        error = "the file is empty";
        return nullptr;
    }
    const std::optional<std::string> tail = span(tailOffset, size_ - 1, error);
    if (!tail) {
        return nullptr;
    }
    const std::optional<zip::Directory> directory = zip::findDirectory(*tail, tailOffset, error);
    if (!directory) {
        return nullptr;
    }
    std::string listing;
    if (directory->offset >= tailOffset) {
        listing = tail->substr(directory->offset - tailOffset, directory->size);
    } else {
        const std::optional<std::string> fetched =
            span(directory->offset, directory->offset + directory->size - 1, error);
        if (!fetched) {
            return nullptr;
        }
        listing = *fetched;
    }
    entries_ = zip::parseDirectory(listing, *directory, error);
    return entries_ ? &*entries_ : nullptr;
}

const zip::Entry* ApkArchive::find(std::string_view name, std::string& error) {
    error.clear();
    const std::vector<zip::Entry>* all = entries(error);
    if (all == nullptr) {
        return nullptr;
    }
    const auto found = std::ranges::find(*all, name, &zip::Entry::name);
    return found == all->end() ? nullptr : &*found;
}

std::optional<std::string> ApkArchive::extract(const zip::Entry& entry, std::string& error,
                                               std::uint32_t maxSize) {
    const std::optional<std::string> header =
        span(entry.localHeaderOffset, entry.localHeaderOffset + zip::localHeaderSize - 1, error);
    if (!header) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> start = zip::dataOffset(entry, *header, error);
    if (!start) {
        return std::nullopt;
    }
    if (entry.compressedSize == 0) {
        return zip::extract(entry, {}, error, maxSize);
    }
    const std::optional<std::string> data = span(*start, *start + entry.compressedSize - 1, error);
    if (!data) {
        return std::nullopt;
    }
    return zip::extract(entry, *data, error, maxSize);
}

ApkFile ApkArchive::fetch(std::string_view name) {
    ApkFile result;
    result.status = ApkFile::Status::Failed;
    const zip::Entry* entry = find(name, result.error);
    if (entry == nullptr) {
        if (result.error.empty()) {
            result.status = ApkFile::Status::Missing;
        }
        return result;
    }
    std::optional<std::string> bytes = extract(*entry, result.error);
    if (!bytes) {
        return result;
    }
    result.status = ApkFile::Status::Found;
    result.bytes = std::move(*bytes);
    return result;
}

} // namespace opensu::artwork
