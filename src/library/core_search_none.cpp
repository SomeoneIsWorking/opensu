#include "libretro_cores.hpp"

namespace opensu::library::roms {

CoreSearch CoreSearch::standard(const std::filesystem::path& /*home*/,
                                const std::filesystem::path& dataDir,
                                const std::filesystem::path& shipped) {
    return CoreSearch{DownloadedCores::under(dataDir).root(), shipped, {}, {}};
}

} // namespace opensu::library::roms
