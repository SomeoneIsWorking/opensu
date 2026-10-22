#include "lossless_scaling.hpp"

#include <system_error>

namespace opensu::library {

std::optional<std::filesystem::path> findLosslessDll(const steam::Library& steam) {
    for (const steam::LibraryFolder& folder : steam.libraryFolders()) {
        std::filesystem::path dll = folder.path / losslessDllInLibrary;
        std::error_code error;
        if (std::filesystem::is_regular_file(dll, error)) {
            return dll;
        }
    }
    return std::nullopt;
}

} // namespace opensu::library
