#include "mgba_saves.hpp"

#include "ini_file.hpp"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;

/// The save RAM's end: mGBA reads a file's size with these bits cleared as its save.
constexpr std::size_t footerBits = 0xFF;

} // namespace

fs::path mgbaSaveFile(const fs::path& config, const fs::path& rom) {
    const fs::path name = rom.stem().string() + ".sav";
    const fs::path named = iniValue(config / "config.ini", "ports.qt", "savegamePath").value_or("");
    if (named.empty()) {
        return rom.parent_path() / name;
    }
    return (named.is_absolute() ? named : config / named) / name;
}

std::optional<std::string> saveRamOfMgba(std::string sav, std::string& error) {
    const std::size_t ram = sav.size() & ~footerBits;
    if (ram == 0) {
        error = "the file is " + std::to_string(sav.size()) + " bytes, too short for a save";
        return std::nullopt;
    }
    sav.resize(ram);
    return sav;
}

std::optional<std::string> mgbaOfSaveRam(std::string ram, std::string& error) {
    if (ram.empty() || (ram.size() & footerBits) != 0) {
        error = "save RAM of " + std::to_string(ram.size()) +
                " bytes would be read by mGBA as another size";
        return std::nullopt;
    }
    return ram;
}

} // namespace opensu::library::saves
