#include "starter_pack.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>

#include <webp/decode.h>

#include "lucent/log.h"
#include "raylib.h"
#include "zip_archive.hpp"

namespace iideck::artwork {
namespace {

// The APK's last bytes, which hold its end record.
constexpr std::uint64_t tailBytes = 64ULL * 1024;
// Each range stays small enough to finish inside the client's timeout.
constexpr std::uint64_t chunkBytes = 4ULL * 1024 * 1024;

struct WebpFree {
    void operator()(std::uint8_t* pixels) const noexcept {
        WebPFree(pixels);
    }
};

struct RaylibFree {
    void operator()(unsigned char* data) const noexcept {
        MemFree(data);
    }
};

std::optional<std::string> readFile(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, {}};
}

/// Bytes `first` to `last` of a URL in `chunkBytes` requests.
std::optional<std::string> fetchSpan(const net::WebClient& web, const std::string& url,
                                     std::uint64_t first, std::uint64_t last, std::string& error) {
    std::string out;
    for (std::uint64_t at = first; at <= last; at += chunkBytes) {
        const std::optional<std::string> part =
            web.getRange(url, at, std::min(at + chunkBytes - 1, last), error);
        if (!part) {
            return std::nullopt;
        }
        out += *part;
    }
    return out;
}

/// A WebP image as PNG bytes.
std::optional<std::string> toPng(std::string_view webp, std::string& error) {
    int width = 0;
    int height = 0;
    const std::unique_ptr<std::uint8_t, WebpFree> pixels{WebPDecodeRGBA(
        reinterpret_cast<const std::uint8_t*>(webp.data()), webp.size(), &width, &height)};
    if (pixels == nullptr) {
        error = "the card is not a WebP image";
        return std::nullopt;
    }
    const Image image{pixels.get(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    int size = 0;
    const std::unique_ptr<unsigned char, RaylibFree> png{ExportImageToMemory(image, ".png", &size)};
    if (png == nullptr || size <= 0) {
        error = "the card cannot be encoded as PNG";
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(png.get()), static_cast<std::size_t>(size)};
}

} // namespace

StarterPack::StarterPack(const ArtworkStore& store, const std::string& apkUrl, const PackPin& pin)
    : store_{store}, apkUrl_{apkUrl}, pin_{pin} {
}

bool StarterPack::ensure(const net::WebClient& web, std::string& error) const {
    std::error_code ec;
    if (std::filesystem::is_regular_file(store_.packPath(pin_.fileName), ec)) {
        return true;
    }
    if (pin_.apkSize < tailBytes) {
        error = "the pinned APK is smaller than its tail";
        return false;
    }
    const std::uint64_t tailOffset = pin_.apkSize - tailBytes;
    const std::optional<std::string> tail =
        fetchSpan(web, apkUrl_, tailOffset, pin_.apkSize - 1, error);
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
        const std::optional<std::string> fetched = fetchSpan(
            web, apkUrl_, directory->offset, directory->offset + directory->size - 1, error);
        if (!fetched) {
            return false;
        }
        listing = *fetched;
    }
    const std::optional<std::vector<zip::Entry>> entries =
        zip::parseDirectory(listing, *directory, error);
    if (!entries) {
        return false;
    }
    const auto found = std::ranges::find(*entries, pin_.entry, &zip::Entry::name);
    if (found == entries->end()) {
        error = std::string{pin_.entry} + " is not in the APK";
        return false;
    }
    if (found->size != pin_.entrySize || found->crc32 != pin_.entryCrc32) {
        error = std::string{pin_.entry} + " in the APK differs from the pinned release";
        return false;
    }
    const std::optional<std::string> header =
        fetchSpan(web, apkUrl_, found->localHeaderOffset,
                  found->localHeaderOffset + zip::localHeaderSize - 1, error);
    if (!header) {
        return false;
    }
    const std::optional<std::uint64_t> start = zip::dataOffset(*found, *header, error);
    if (!start) {
        return false;
    }
    const std::optional<std::string> data =
        fetchSpan(web, apkUrl_, *start, *start + found->compressedSize - 1, error);
    if (!data) {
        return false;
    }
    const std::optional<std::string> pack = zip::extract(*found, *data, error);
    if (!pack || !store_.savePack(pin_.fileName, *pack, error)) {
        return false;
    }
    lucent::info("artwork", "starter pack stored: {} bytes", pack->size());
    return true;
}

std::optional<std::string> StarterPack::card(std::string_view system, std::string& error) const {
    error.clear();
    const std::filesystem::path file = store_.packPath(pin_.fileName);
    std::optional<std::string> bytes = readFile(file);
    if (!bytes) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    const std::optional<zip::Archive> pack = zip::Archive::open(std::move(*bytes), error);
    if (!pack) {
        return std::nullopt;
    }
    const std::optional<std::string> webp =
        pack->read("platforms/" + std::string{system} + ".webp", error);
    if (!webp) {
        return std::nullopt;
    }
    return toPng(*webp, error);
}

} // namespace iideck::artwork
