#include "starter_pack.hpp"

#include <cstddef>
#include <fstream>
#include <span>
#include <utility>

#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::artwork {
namespace {

std::optional<std::string> readFile(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, {}};
}

/// A WebP image as PNG bytes.
std::optional<std::string> toPng(std::string_view webp, std::string& error) {
    std::string reason;
    const std::optional<gpu2d::image::Bitmap> card =
        gpu2d::image::decode(std::as_bytes(std::span{webp}), reason);
    if (!card) {
        error = "the card is not a WebP image: " + reason;
        return std::nullopt;
    }
    std::string png = gpu2d::image::encodePng(card->rgba, static_cast<unsigned>(card->width),
                                              static_cast<unsigned>(card->height), reason);
    if (png.empty()) {
        error = "the card cannot be encoded as PNG: " + reason;
        return std::nullopt;
    }
    return png;
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

} // namespace opensu::artwork
