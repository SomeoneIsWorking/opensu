#include "arcade_dat.hpp"

#include <algorithm>
#include <string>

#include "libretro_index.hpp"
#include "zip/zip_archive.hpp"

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
        if (bytes->size() != pin.size || zip::crc32Of(*bytes) != pin.crc32) {
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
