#include "save_memory.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::retro {

SaveMemory::SaveMemory(std::filesystem::path file) : file_{std::move(file)} {
}

void SaveMemory::load(std::span<std::byte> ram) {
    written_.assign(ram.begin(), ram.end());
    std::error_code error;
    const auto size = std::filesystem::file_size(file_, error);
    if (error) {
        lucent::info("retro", "no save at {}", file_.string());
        return;
    }
    if (size != ram.size()) {
        lucent::warn("retro", "{} is {} bytes, the core's save RAM {}; starting blank",
                     file_.string(), size, ram.size());
        return;
    }
    std::ifstream in{file_, std::ios::binary};
    std::vector<char> bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (bytes.size() != ram.size()) {
        lucent::error("retro", "cannot read {}; starting blank", file_.string());
        return;
    }
    std::ranges::transform(bytes, ram.begin(), [](char byte) {
        return static_cast<std::byte>(byte);
    });
    written_.assign(ram.begin(), ram.end());
    lucent::info("retro", "loaded save {}", file_.string());
}

bool SaveMemory::flush(std::span<const std::byte> ram) {
    if (std::ranges::equal(ram, written_)) {
        return true;
    }
    std::string error;
    const std::string_view bytes{reinterpret_cast<const char*>(ram.data()), ram.size()};
    if (!fileio::writeWhole(file_, bytes, error)) {
        lucent::error("retro", "save not written: {}", error);
        return false;
    }
    written_.assign(ram.begin(), ram.end());
    lucent::info("retro", "wrote save {}", file_.string());
    return true;
}

} // namespace opensu::retro
