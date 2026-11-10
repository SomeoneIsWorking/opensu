#include "libretro_cores.hpp"

namespace opensu::library::roms {

CoreSearch CoreSearch::standard(const std::filesystem::path& home,
                                const std::filesystem::path& dataDir) {
    return CoreSearch{DownloadedCores::under(dataDir).root(),
                      {home / ".config" / "retroarch" / "cores", "/usr/lib64/libretro",
                       "/usr/lib/libretro", "/usr/lib/x86_64-linux-gnu/libretro"},
                      {home / ".config" / "retroarch" / "info", "/usr/share/libretro/info"}};
}

} // namespace opensu::library::roms
