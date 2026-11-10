#include "libretro_cores.hpp"

namespace opensu::library::roms {

CoreSearch CoreSearch::standard(const std::filesystem::path& /*home*/,
                                const std::filesystem::path& dataDir) {
    return CoreSearch{DownloadedCores::under(dataDir).root(), {}, {}};
}

} // namespace opensu::library::roms
