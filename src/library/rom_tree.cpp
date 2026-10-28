#include "rom_tree.hpp"

#include <algorithm>
#include <system_error>

namespace opensu::library::roms {

namespace fs = std::filesystem;

RomNode FileTree::node(const fs::path& path) {
    std::error_code ec;
    RomNode node{path, path.filename().string(), fs::is_directory(path, ec)};
    if (!node.folder) {
        const std::uintmax_t size = fs::file_size(path, ec);
        node.size = ec ? 0 : size;
    }
    return node;
}

std::optional<std::vector<RomNode>> FileTree::children(const fs::path& folder) const {
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) {
        return std::nullopt;
    }
    std::vector<RomNode> nodes;
    for (const fs::directory_entry& entry : fs::directory_iterator{folder, ec}) {
        nodes.push_back(node(entry.path()));
    }
    std::ranges::sort(nodes, {}, &RomNode::location);
    return nodes;
}

} // namespace opensu::library::roms
