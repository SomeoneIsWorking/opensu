#include "save_formats.hpp"

#include "mgba_saves.hpp"
#include "ps1_cards.hpp"

namespace opensu::library::saves {
namespace {

/// `bytes` in `format` as the core's save RAM.
std::optional<std::string> saveRamOf(SaveFormat format, std::string bytes, std::string& error) {
    switch (format) {
    case SaveFormat::Mgba:
        return saveRamOfMgba(std::move(bytes), error);
    case SaveFormat::DuckStationCard:
    case SaveFormat::SaveRam:
    case SaveFormat::AsIs:
        break;
    }
    return bytes;
}

/// The core's save RAM `ram` in `format`.
std::optional<std::string> formatted(SaveFormat format, std::string ram, std::string& error) {
    switch (format) {
    case SaveFormat::Mgba:
        return mgbaOfSaveRam(std::move(ram), error);
    case SaveFormat::DuckStationCard:
        return wholeCard(std::move(ram), error);
    case SaveFormat::SaveRam:
    case SaveFormat::AsIs:
        break;
    }
    return ram;
}

} // namespace

bool reformatted(SaveFormat from, SaveFormat to) noexcept {
    return from != to && from != SaveFormat::AsIs && to != SaveFormat::AsIs;
}

std::optional<std::string> converted(SaveFormat from, SaveFormat to, std::string bytes,
                                     std::string& error) {
    std::optional<std::string> ram = saveRamOf(from, std::move(bytes), error);
    if (!ram) {
        return std::nullopt;
    }
    return formatted(to, std::move(*ram), error);
}

} // namespace opensu::library::saves
