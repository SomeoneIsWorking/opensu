#include "dump_info.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>
#include <system_error>

#include "param_sfo.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// A param.sfo is a few kilobytes; anything far larger is not one.
constexpr std::uintmax_t largestSfo = std::uintmax_t{64} * 1024;

constexpr std::array<std::string_view, 2> trademarkSigns{"™", "®"};

std::string withoutTrademarks(std::string title) {
    for (const std::string_view sign : trademarkSigns) {
        for (std::size_t at = title.find(sign); at != std::string::npos; at = title.find(sign)) {
            title.erase(at, sign.size());
        }
    }
    return title;
}

std::string readSmall(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || fs::file_size(file, ec) > largestSfo) {
        return {};
    }
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

} // namespace

std::optional<DumpInfo> readDumpInfo(const RomSystem& system, const fs::path& entry) {
    std::error_code ec;
    if (system.key != "ps4" || !fs::is_directory(entry, ec)) {
        return std::nullopt;
    }
    const fs::path meta = entry / "sce_sys";
    DumpInfo info;
    const std::map<std::string, std::string> values = sfoStrings(readSmall(meta / "param.sfo"));
    if (const auto title = values.find("TITLE"); title != values.end()) {
        info.title = withoutTrademarks(title->second);
    }
    if (fs::is_regular_file(meta / "icon0.png", ec)) {
        info.icon = meta / "icon0.png";
    }
    if (info.title.empty() && info.icon.empty()) {
        return std::nullopt;
    }
    return info;
}

} // namespace opensu::library::roms
