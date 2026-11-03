#include "save_paths.hpp"

namespace opensu::retro {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view coreSuffix = "_libretro";

fs::path named(const fs::path& folder, const fs::path& rom, std::string_view extension) {
    return folder / (rom.stem().string() + std::string{extension});
}

} // namespace

fs::path systemSaveFolder(const fs::path& saves, std::string_view system) {
    return saves / std::string{system};
}

fs::path coreSaveFolder(const fs::path& saves, const fs::path& core) {
    return saves / coreName(core);
}

std::string coreName(const fs::path& core) {
    std::string name = core.stem().string();
    if (const std::size_t at = name.rfind(coreSuffix); at != std::string::npos) {
        name.erase(at);
    }
    return name;
}

fs::path saveMemoryFile(const fs::path& folder, const fs::path& rom) {
    return named(folder, rom, ".srm");
}

fs::path saveStateFile(const fs::path& folder, const fs::path& rom) {
    return named(folder, rom, ".state");
}

} // namespace opensu::retro
