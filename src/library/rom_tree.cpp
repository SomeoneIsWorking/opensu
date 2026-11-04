#include "rom_tree.hpp"

#include <algorithm>
#include <system_error>

#include "fileio/folder_access.hpp"

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

std::string FileTree::label(const fs::path& folder) const {
    return folder.string();
}

std::string FileTree::writeRefusal(const fs::path& folder) const {
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) {
        return folder.string() + " is not a folder";
    }
    if (!fileio::mayUseFolder(folder, fileio::FolderUse::Write)) {
        return folder.string() + " cannot be written to";
    }
    return {};
}

bool FileTree::place(const fs::path& folder, std::string_view relative, const fs::path& staged,
                     std::string& error) const {
    const fs::path target = folder / fs::path{std::string{relative}};
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    if (ec) {
        error = "cannot make " + target.parent_path().string() + ": " + ec.message();
        return false;
    }
    fs::rename(staged, target, ec);
    if (!ec) {
        return true;
    }
    // Another filesystem: copied beside the target first, so a half copy is never taken for it.
    fs::path copy = target;
    copy += ".part";
    fs::copy_file(staged, copy, fs::copy_options::overwrite_existing, ec);
    if (!ec) {
        fs::rename(copy, target, ec);
    }
    if (ec) {
        fs::remove(copy, ec);
        error = "cannot put " + target.string() + " in place";
        return false;
    }
    fs::remove(staged, ec);
    return true;
}

} // namespace opensu::library::roms
