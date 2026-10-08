#include "apk_archive.hpp"

#include <algorithm>

namespace iideck::artwork {
namespace {

// The APK's last bytes, which hold its end record.
constexpr std::uint64_t tailBytes = 64ULL * 1024;
// Each range stays small enough to finish inside the client's timeout.
constexpr std::uint64_t chunkBytes = 4ULL * 1024 * 1024;

} // namespace

ApkArchive::ApkArchive(std::string url, std::uint64_t size) : url_{std::move(url)}, size_{size} {
}

std::optional<std::string> ApkArchive::span(const net::WebClient& web, std::uint64_t first,
                                            std::uint64_t last, std::string& error) const {
    std::string out;
    for (std::uint64_t at = first; at <= last; at += chunkBytes) {
        const std::optional<std::string> part =
            web.getRange(url_, at, std::min(at + chunkBytes - 1, last), error);
        if (!part) {
            return std::nullopt;
        }
        out += *part;
    }
    return out;
}

bool ApkArchive::load(const net::WebClient& web, std::string& error) {
    if (entries_) {
        return true;
    }
    if (size_ < tailBytes) {
        error = "the pinned APK is smaller than its tail";
        return false;
    }
    const std::uint64_t tailOffset = size_ - tailBytes;
    const std::optional<std::string> tail = span(web, tailOffset, size_ - 1, error);
    if (!tail) {
        return false;
    }
    const std::optional<zip::Directory> directory = zip::findDirectory(*tail, tailOffset, error);
    if (!directory) {
        return false;
    }
    std::string listing;
    if (directory->offset >= tailOffset) {
        listing = tail->substr(directory->offset - tailOffset, directory->size);
    } else {
        const std::optional<std::string> fetched =
            span(web, directory->offset, directory->offset + directory->size - 1, error);
        if (!fetched) {
            return false;
        }
        listing = *fetched;
    }
    entries_ = zip::parseDirectory(listing, *directory, error);
    return entries_.has_value();
}

const zip::Entry* ApkArchive::find(const net::WebClient& web, std::string_view name,
                                   std::string& error) {
    error.clear();
    if (!load(web, error) || !entries_) {
        return nullptr;
    }
    const auto found = std::ranges::find(*entries_, name, &zip::Entry::name);
    return found == entries_->end() ? nullptr : &*found;
}

std::optional<std::string> ApkArchive::extract(const net::WebClient& web, const zip::Entry& entry,
                                               std::string& error) {
    const std::optional<std::string> header = span(
        web, entry.localHeaderOffset, entry.localHeaderOffset + zip::localHeaderSize - 1, error);
    if (!header) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> start = zip::dataOffset(entry, *header, error);
    if (!start) {
        return std::nullopt;
    }
    if (entry.compressedSize == 0) {
        return zip::extract(entry, {}, error);
    }
    const std::optional<std::string> data =
        span(web, *start, *start + entry.compressedSize - 1, error);
    if (!data) {
        return std::nullopt;
    }
    return zip::extract(entry, *data, error);
}

ApkFile ApkArchive::fetch(const net::WebClient& web, std::string_view name) {
    ApkFile result;
    result.status = ApkFile::Status::Failed;
    const zip::Entry* entry = find(web, name, result.error);
    if (entry == nullptr) {
        if (result.error.empty()) {
            result.status = ApkFile::Status::Missing;
        }
        return result;
    }
    std::optional<std::string> bytes = extract(web, *entry, result.error);
    if (!bytes) {
        return result;
    }
    result.status = ApkFile::Status::Found;
    result.bytes = std::move(*bytes);
    return result;
}

} // namespace iideck::artwork
