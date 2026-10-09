#include "atomic_write.hpp"

#include <fstream>
#include <system_error>

namespace opensu::fileio {

bool writeWhole(const std::filesystem::path& file, std::string_view bytes, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::filesystem::path partial = file;
    partial += ".part";
    {
        std::ofstream out{partial, std::ios::binary | std::ios::trunc};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            error = "cannot write " + partial.string();
            return false;
        }
    }
    std::filesystem::rename(partial, file, ec);
    if (ec) {
        error = "cannot replace " + file.string() + ": " + ec.message();
        return false;
    }
    return true;
}

} // namespace opensu::fileio
