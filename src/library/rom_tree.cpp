#include "rom_tree.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

#include "fileio/folder_access.hpp"

namespace opensu::library::roms {

namespace fs = std::filesystem;

namespace {

class LocalFile final : public RomFile {
  public:
    LocalFile(const fs::path& file, std::uint64_t size) : in_{file, std::ios::binary}, size_{size} {
    }

    [[nodiscard]] bool opened() const {
        return in_.is_open();
    }
    [[nodiscard]] std::uint64_t size() const override {
        return size_;
    }
    [[nodiscard]] std::optional<std::string> read(std::uint64_t offset, std::size_t size) override {
        std::string bytes(size, '\0');
        in_.clear();
        in_.seekg(static_cast<std::streamoff>(offset));
        in_.read(bytes.data(), static_cast<std::streamsize>(size));
        if (!in_ || static_cast<std::size_t>(in_.gcount()) != size) {
            return std::nullopt;
        }
        return bytes;
    }

  private:
    std::ifstream in_;
    std::uint64_t size_;
};

} // namespace

RomNode FileTree::node(const fs::path& path) {
    std::error_code ec;
    RomNode node{path, path.filename().string(), fs::is_directory(path, ec)};
    if (!node.folder) {
        const std::uintmax_t size = fs::file_size(path, ec);
        node.size = ec ? 0 : size;
        const fs::file_time_type modified = fs::last_write_time(path, ec);
        node.modified = ec ? 0 : static_cast<std::int64_t>(modified.time_since_epoch().count());
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

std::unique_ptr<RomFile> FileTree::open(const fs::path& file) const {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    if (ec) {
        return nullptr;
    }
    auto opened = std::make_unique<LocalFile>(file, size);
    if (!opened->opened()) {
        return nullptr;
    }
    return opened;
}

std::string FileTree::label(const fs::path& folder) const {
    return folder.string();
}

void FileTree::release(const fs::path& /*folder*/) const {
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
