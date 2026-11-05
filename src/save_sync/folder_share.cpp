#include "folder_share.hpp"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"

namespace opensu::save_sync {

namespace fs = std::filesystem;

FolderShare::FolderShare(fs::path root) : root_{std::move(root)} {
}

std::string FolderShare::where() const {
    return root_.string();
}

bool FolderShare::reachable() const {
    std::error_code ec;
    return fs::is_directory(root_, ec);
}

SaveShare::Read FolderShare::read(const std::stop_token& /*stop*/, const std::string& name) const {
    const fs::path file = root_ / name;
    std::error_code ec;
    if (!fs::exists(file, ec)) {
        return Read{.kind = ec ? Read::Kind::Failed : Read::Kind::Missing, .bytes = ec.message()};
    }
    std::ifstream in{file, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (!in.good() && !in.eof()) {
        return Read{.kind = Read::Kind::Failed, .bytes = file.string() + " cannot be read"};
    }
    return Read{.kind = Read::Kind::Found, .bytes = std::move(bytes)};
}

bool FolderShare::write(const std::stop_token& /*stop*/, const std::string& name,
                        const std::string& bytes, std::string& error) const {
    return fileio::writeWhole(root_ / name, bytes, error);
}

} // namespace opensu::save_sync
