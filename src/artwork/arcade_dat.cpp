#include "arcade_dat.hpp"

#include <zlib.h>

#include <algorithm>
#include <string>

#include "libretro_index.hpp"

namespace opensu::artwork {
namespace {

std::string urlOf(std::string_view base, std::string_view path) {
    std::string url{base};
    url += '/';
    url += library::roms::libretroDatabaseRevision;
    url += "/metadat";
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = std::min(path.find('/', start), path.size());
        url += '/';
        url += encodeSegment(path.substr(start, slash - start));
        start = slash + 1;
    }
    return url;
}

std::uint32_t crc32Of(std::string_view bytes) {
    return static_cast<std::uint32_t>(
        ::crc32(0L, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uInt>(bytes.size())));
}

} // namespace

std::optional<library::roms::NameMap> fetchArcadeNames(const net::WebClient& web,
                                                       std::string_view base,
                                                       std::span<const DatPin> pins,
                                                       std::string& error) {
    library::roms::NameMap names;
    for (const DatPin& pin : pins) {
        const std::string url = urlOf(base, pin.path);
        const std::optional<std::string> bytes = web.get(url, error);
        if (!bytes) {
            error.insert(0, url + ": ");
            return std::nullopt;
        }
        if (bytes->size() != pin.size || crc32Of(*bytes) != pin.crc32) {
            error = url + " differs from its pin";
            return std::nullopt;
        }
        for (auto& [key, title] : library::roms::parseDat(*bytes)) {
            names.try_emplace(key, std::move(title));
        }
    }
    return names;
}

} // namespace opensu::artwork
