#include "starter_pack.hpp"

#include <fstream>
#include <memory>
#include <utility>

#include <webp/decode.h>

#include "lucent/log.h"
#include "raylib.h"
#include "zip_archive.hpp"

namespace iideck::artwork {
namespace {

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

StarterPack::StarterPack(const ArtworkStore& store, ApkArchive& apk, const PackPin& pin)
    : store_{store}, apk_{apk}, pin_{pin} {
}

bool StarterPack::ensure(const net::WebClient& web, std::string& error) const {
    std::error_code ec;
    if (std::filesystem::is_regular_file(store_.packPath(pin_.fileName), ec)) {
        return true;
    }
    const zip::Entry* entry = apk_.find(web, pin_.entry, error);
    if (entry == nullptr) {
        if (error.empty()) {
            error = std::string{pin_.entry} + " is not in the APK";
        }
        return false;
    }
    if (entry->size != pin_.entrySize || entry->crc32 != pin_.entryCrc32) {
        error = std::string{pin_.entry} + " in the APK differs from the pinned release";
        return false;
    }
    const std::optional<std::string> pack = apk_.extract(web, *entry, error);
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
