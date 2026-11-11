#include "lrps2_card.hpp"

#include <system_error>

#include "lucent/log.h"
#include "save_paths.hpp"

namespace opensu::retro {

namespace fs = std::filesystem;

fs::path lrps2SharedCard(const fs::path& system) {
    return system / "pcsx2" / "memcards" / "Mcd001.ps2";
}

bool adoptLrps2SharedCard(const fs::path& system, const fs::path& saves, const fs::path& rom) {
    const fs::path card = lrps2CardFile(saves, rom);
    const fs::path shared = lrps2SharedCard(system);
    std::error_code ec;
    if (fs::exists(card, ec) || ec || !fs::is_regular_file(shared, ec)) {
        return false;
    }
    const fs::path part = card.string() + ".part";
    fs::copy_file(shared, part, fs::copy_options::overwrite_existing, ec);
    if (!ec) {
        // Before the core loads nothing else writes the card.
        fs::rename(part, card, ec);
    }
    std::error_code ignored;
    fs::remove(part, ignored);
    if (ec) {
        lucent::warn("retro", "the shared card {} cannot be copied to {}: {}", shared.string(),
                     card.string(), ec.message());
        return false;
    }
    lucent::info("retro", "{} starts from a copy of the shared card {}", card.string(),
                 shared.string());
    return true;
}

} // namespace opensu::retro
