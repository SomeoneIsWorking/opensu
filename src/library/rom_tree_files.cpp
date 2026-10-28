#include "rom_tree.hpp"

namespace opensu::library::roms {

std::unique_ptr<RomTree> openRomTree() {
    return std::make_unique<FileTree>();
}

} // namespace opensu::library::roms
