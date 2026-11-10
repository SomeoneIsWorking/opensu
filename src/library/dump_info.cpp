#include "dump_info.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>
#include <system_error>

#include "param_sfo.hpp"
#include "rom_titles.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// A param.sfo is a few kilobytes; anything far larger is not one.
constexpr std::uintmax_t largestSfo = std::uintmax_t{64} * 1024;

std::string readSmall(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || fs::file_size(file, ec) > largestSfo) {
        return {};
    }
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

// A Wii disc header: the game id at its start, the Wii magic word at 0x18.
constexpr std::size_t gameIdLength = 6;
constexpr std::size_t wiiMagicAt = 0x18;
constexpr std::array<unsigned char, 4> wiiMagic{0x5D, 0x1C, 0x9E, 0xA3};
// An RVZ or WIA file keeps the disc header in its second header, at 0x58.
constexpr std::size_t wiaHeaderAt = 0x58;
// A WBFS file's first disc starts at its second sector; the sector size's shift is at byte 8.
constexpr std::size_t wbfsSectorShiftAt = 8;
constexpr unsigned largestWbfsShift = 16;
constexpr std::size_t headerRead = 0x80;

std::string readAt(std::ifstream& in, std::size_t at, std::size_t length) {
    std::string bytes(length, '\0');
    in.seekg(static_cast<std::streamoff>(at));
    in.read(bytes.data(), static_cast<std::streamsize>(length));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    in.clear();
    return bytes;
}

/// Where the disc header starts in the image `start` begins, by its container's magic.
std::optional<std::size_t> discHeaderAt(std::string_view start) {
    if (start.starts_with("RVZ\x01") || start.starts_with("WIA\x01")) {
        return wiaHeaderAt;
    }
    if (start.starts_with("WBFS")) {
        if (start.size() <= wbfsSectorShiftAt) {
            return std::nullopt;
        }
        const auto shift = static_cast<unsigned char>(start[wbfsSectorShiftAt]);
        return shift <= largestWbfsShift ? std::optional{std::size_t{1} << shift} : std::nullopt;
    }
    return std::size_t{0};
}

/// The game id of the Wii disc image `file`, or empty when it is none.
std::string wiiGameId(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    const std::optional<std::size_t> at = discHeaderAt(readAt(in, 0, headerRead));
    if (!at) {
        return {};
    }
    const std::string header = readAt(in, *at, wiiMagicAt + wiiMagic.size());
    const bool wii = header.size() == wiiMagicAt + wiiMagic.size() &&
                     std::ranges::equal(std::string_view{header}.substr(wiiMagicAt), wiiMagic,
                                        [](char got, unsigned char want) {
                                            return static_cast<unsigned char>(got) == want;
                                        });
    std::string id = header.substr(0, gameIdLength);
    if (!wii || !std::ranges::all_of(id, [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        })) {
        return {};
    }
    return id;
}

} // namespace

std::optional<DumpInfo> readDumpInfo(const RomSystem& system, const fs::path& entry) {
    std::error_code ec;
    if (system.key == "wii" && fs::is_regular_file(entry, ec)) {
        std::string id = wiiGameId(entry);
        if (id.empty()) {
            return std::nullopt;
        }
        return DumpInfo{.title = {}, .icon = {}, .titleId = std::move(id), .saveData = {}};
    }
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
