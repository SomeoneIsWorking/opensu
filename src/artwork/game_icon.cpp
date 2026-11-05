#include "game_icon.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>

#include "fileio/checked_bytes.hpp"
#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "library/pe_resources.hpp"
#include "lucent/log.h"

namespace opensu::artwork {
namespace {

namespace fs = std::filesystem;
namespace image = gpu2d::image;
using Bytes = std::span<const std::uint8_t>;
using fileio::CheckedBytes;

/// The largest side an icon may claim: Apple's largest is 1024.
constexpr std::int32_t mostSide = 1024;
/// The most bytes of an icon file read.
constexpr std::size_t mostIconBytes = 32U << 20U;
constexpr std::array<std::uint8_t, 8> pngSignature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::size_t icoEntrySize = 16;
constexpr std::size_t groupEntrySize = 14;
constexpr std::size_t iconDirSize = 6;
constexpr std::uint32_t uncompressed = 0;
constexpr std::uint32_t bitFields = 3;

bool isPng(Bytes bytes) {
    return bytes.size() > pngSignature.size() &&
           std::ranges::equal(bytes.first(pngSignature.size()), pngSignature);
}

std::uint32_t bigEndian(Bytes bytes, std::size_t at) {
    return (std::uint32_t{bytes[at]} << 24U) | (std::uint32_t{bytes[at + 1]} << 16U) |
           (std::uint32_t{bytes[at + 2]} << 8U) | bytes[at + 3];
}

/// `bytes` as PNG when they are one a decoder takes, its size checked before it is decoded.
std::optional<std::string> checkedPng(Bytes bytes) {
    constexpr std::size_t widthAt = 16;
    if (!isPng(bytes) || bytes.size() < widthAt + 8) {
        return std::nullopt;
    }
    const std::uint32_t width = bigEndian(bytes, widthAt);
    const std::uint32_t height = bigEndian(bytes, widthAt + 4);
    if (width == 0 || height == 0 || width > mostSide || height > mostSide) {
        return std::nullopt;
    }
    std::string error;
    if (!image::decode(std::as_bytes(bytes), error)) {
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

/// What an icon's DIB is laid out as.
struct Dib {
    std::int32_t width;
    std::int32_t height;
    std::uint32_t depth;
    std::size_t palette;
    std::size_t pixels;
    std::size_t stride;
    std::size_t mask;
    std::size_t maskStride;
};

/// The layout `bytes` claim; nothing for one that does not fit them.
std::optional<Dib> dibOf(const CheckedBytes& bytes) {
    const auto header = bytes.u32(0);
    const auto width = bytes.u32(4);
    const auto doubled = bytes.u32(8);
    const auto depth = bytes.u16(14);
    const auto compression = bytes.u32(16);
    const auto used = bytes.u32(32);
    if (!header || *header < 40 || !width || !doubled || !depth || !compression || !used) {
        return std::nullopt;
    }
    const auto side = static_cast<std::int32_t>(*width);
    // An icon's DIB counts its colour rows and its mask's.
    const std::int32_t rows = static_cast<std::int32_t>(*doubled) / 2;
    if (side <= 0 || side > mostSide || rows <= 0 || rows > mostSide) {
        return std::nullopt;
    }
    constexpr std::array<std::uint32_t, 5> depths{1, 4, 8, 24, 32};
    if (std::ranges::find(depths, std::uint32_t{*depth}) == depths.end() ||
        (*compression != uncompressed && !(*compression == bitFields && *depth == 32))) {
        return std::nullopt;
    }
    const std::size_t most = *depth <= 8 ? std::size_t{1} << *depth : 0;
    const std::size_t colours = *used != 0 ? std::min<std::size_t>(*used, most) : most;
    Dib dib{.width = side,
            .height = rows,
            .depth = *depth,
            .palette = *header,
            .pixels = *header + (colours * 4),
            .stride = ((static_cast<std::size_t>(side) * *depth + 31) / 32) * 4,
            .mask = 0,
            .maskStride = ((static_cast<std::size_t>(side) + 31) / 32) * 4};
    dib.mask = dib.pixels + (dib.stride * static_cast<std::size_t>(rows));
    if (!bytes.holds(dib.pixels, dib.stride * static_cast<std::size_t>(rows))) {
        return std::nullopt;
    }
    return dib;
}

/// Colour (b, g, r, a) of pixel `x` in the row at `row`.
std::array<std::uint8_t, 4> pixelOf(const CheckedBytes& bytes, const Dib& dib, std::size_t row,
                                    std::size_t x) {
    if (dib.depth >= 24) {
        const std::size_t at = row + (x * (dib.depth / 8));
        return {bytes.u8(at).value_or(0), bytes.u8(at + 1).value_or(0),
                bytes.u8(at + 2).value_or(0),
                dib.depth == 32 ? bytes.u8(at + 3).value_or(0) : std::uint8_t{0}};
    }
    const std::size_t bit = x * dib.depth;
    const std::uint32_t byte = bytes.u8(row + (bit / 8)).value_or(0);
    const std::uint32_t shift = 8U - dib.depth - static_cast<std::uint32_t>(bit % 8);
    const std::uint32_t index = (byte >> shift) & ((1U << dib.depth) - 1U);
    const std::size_t at = dib.palette + (std::size_t{index} * 4);
    return {bytes.u8(at).value_or(0), bytes.u8(at + 1).value_or(0), bytes.u8(at + 2).value_or(0),
            0};
}

/// An icon's device-independent bitmap, palette and mask applied, as PNG.
std::optional<std::string> pngOfDib(Bytes raw) {
    const CheckedBytes bytes{raw};
    const std::optional<Dib> dib = dibOf(bytes);
    if (!dib) {
        return std::nullopt;
    }
    const auto width = static_cast<std::size_t>(dib->width);
    const auto height = static_cast<std::size_t>(dib->height);
    const bool masked = bytes.holds(dib->mask, dib->maskStride * height);
    std::vector<std::uint8_t> rgba(width * height * 4);
    bool anyAlpha = false;
    for (std::size_t y = 0; y < height; ++y) {
        // Rows run bottom first.
        const std::size_t source = height - 1 - y;
        const std::size_t row = dib->pixels + (source * dib->stride);
        for (std::size_t x = 0; x < width; ++x) {
            const std::array<std::uint8_t, 4> colour = pixelOf(bytes, *dib, row, x);
            std::uint8_t* out = &rgba[((y * width) + x) * 4];
            out[0] = colour[2];
            out[1] = colour[1];
            out[2] = colour[0];
            out[3] = colour[3];
            anyAlpha = anyAlpha || colour[3] != 0;
        }
    }
    if (!anyAlpha) {
        // No alpha of its own: the AND mask says what shows, a set bit being clear.
        for (std::size_t y = 0; y < height; ++y) {
            const std::size_t row = dib->mask + ((height - 1 - y) * dib->maskStride);
            for (std::size_t x = 0; x < width; ++x) {
                const std::uint32_t byte = masked ? bytes.u8(row + (x / 8)).value_or(0) : 0;
                const bool clear = ((byte >> (7U - (x % 8))) & 1U) != 0;
                rgba[(((y * width) + x) * 4) + 3] = clear ? 0 : 255;
            }
        }
    }
    std::string error;
    std::string png =
        image::encodePng(rgba, static_cast<unsigned>(width), static_cast<unsigned>(height), error);
    if (png.empty()) {
        lucent::warn("artwork", "icon: {}", error);
        return std::nullopt;
    }
    return png;
}

std::optional<std::string> pngOfImage(Bytes image) {
    return isPng(image) ? checkedPng(image) : pngOfDib(image);
}

/// One image an icon directory lists.
struct Listed {
    /// Its side, 0 standing for 256.
    std::uint32_t side;
    std::uint32_t depth;
    Bytes bytes;
};

/// Largest first, deepest colour first among equals.
void sortBest(std::vector<Listed>& images) {
    std::ranges::stable_sort(images, [](const Listed& a, const Listed& b) {
        return a.side != b.side ? a.side > b.side : a.depth > b.depth;
    });
}

/// The first of `images` that decodes.
std::optional<std::string> bestOf(std::vector<Listed> images) {
    sortBest(images);
    for (const Listed& image : images) {
        if (std::optional<std::string> png = pngOfImage(image.bytes)) {
            return png;
        }
    }
    return std::nullopt;
}

std::uint32_t sideOf(std::uint8_t stored) {
    return stored == 0 ? 256 : stored;
}

std::vector<std::uint8_t> fileBytes(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    std::error_code error;
    const std::uintmax_t size = fs::file_size(file, error);
    std::vector<std::uint8_t> bytes(error ? 0 : std::min<std::uintmax_t>(size, mostIconBytes));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    return bytes;
}

std::string extensionOf(const fs::path& file) {
    std::string extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

bool isFile(const fs::path& file) {
    std::error_code error;
    return fs::is_regular_file(file, error);
}

/// The icon in `file`, by what it is.
std::optional<std::string> iconIn(const fs::path& file, std::int32_t index) {
    const std::string extension = extensionOf(file);
    if (extension == ".exe" || extension == ".dll") {
        return iconOfProgram(file, index);
    }
    const std::vector<std::uint8_t> bytes = fileBytes(file);
    if (extension == ".ico") {
        return iconOfIco(bytes);
    }
    if (extension == ".icns") {
        return iconOfIcns(bytes);
    }
    if (isPng(bytes)) {
        return checkedPng(bytes);
    }
    std::string error;
    const std::optional<image::Bitmap> picture =
        image::decode(std::as_bytes(std::span{bytes}), error);
    if (!picture || picture->width > mostSide || picture->height > mostSide) {
        return std::nullopt;
    }
    std::string png = image::encodePng(picture->rgba, static_cast<unsigned>(picture->width),
                                       static_cast<unsigned>(picture->height), error);
    return png.empty() ? std::nullopt : std::optional{std::move(png)};
}

/// The file of theme icon `name` in `themes`: the largest hicolor PNG, else a pixmap.
fs::path themeIcon(const std::string& name, const std::vector<fs::path>& themes) {
    constexpr std::array<std::string_view, 7> sizes{"1024x1024", "512x512", "256x256", "128x128",
                                                    "96x96",     "64x64",   "48x48"};
    for (const fs::path& root : themes) {
        for (const std::string_view size : sizes) {
            const fs::path file = root / "hicolor" / size / "apps" / (name + ".png");
            if (isFile(file)) {
                return file;
            }
        }
        if (const fs::path pixmap = root.parent_path() / "pixmaps" / (name + ".png");
            isFile(pixmap)) {
            return pixmap;
        }
    }
    return {};
}

/// An `.ico` beside `file`, the first by name.
fs::path icoBeside(const fs::path& file) {
    std::vector<fs::path> found;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{file.parent_path(), error}) {
        if (extensionOf(entry.path()) == ".ico") {
            found.push_back(entry.path());
        }
    }
    return found.empty() ? fs::path{} : *std::ranges::min_element(found);
}

} // namespace

std::optional<std::string> iconOfIco(Bytes raw) {
    const CheckedBytes bytes{raw};
    const auto count = bytes.u16(4);
    if (bytes.u16(0) != 0 || bytes.u16(2) != 1 || !count) {
        return std::nullopt;
    }
    std::vector<Listed> images;
    for (std::size_t i = 0; i < *count; ++i) {
        const std::size_t at = iconDirSize + (i * icoEntrySize);
        const auto side = bytes.u8(at);
        const auto depth = bytes.u16(at + 6);
        const auto size = bytes.u32(at + 8);
        const auto offset = bytes.u32(at + 12);
        if (!side || !depth || !size || !offset) {
            break;
        }
        if (const auto image = bytes.slice(*offset, *size)) {
            images.push_back(Listed{sideOf(*side), *depth, *image});
        }
    }
    return bestOf(std::move(images));
}

std::optional<std::string> iconOfIcns(Bytes bytes) {
    constexpr std::size_t headerSize = 8;
    if (bytes.size() < headerSize ||
        std::string_view{reinterpret_cast<const char*>(bytes.data()), 4} != "icns") {
        return std::nullopt;
    }
    std::optional<std::string> best;
    std::uint32_t bestSide = 0;
    for (std::size_t at = headerSize; at + headerSize <= bytes.size();) {
        const std::uint32_t size = bigEndian(bytes, at + 4);
        if (size < headerSize || size > bytes.size() - at) {
            break;
        }
        const Bytes payload = bytes.subspan(at + headerSize, size - headerSize);
        if (isPng(payload) && payload.size() >= 24) {
            const std::uint32_t side = bigEndian(payload, 16);
            if (side > bestSide) {
                if (std::optional<std::string> png = checkedPng(payload)) {
                    best = std::move(png);
                    bestSide = side;
                }
            }
        }
        at += size;
    }
    return best;
}

std::optional<std::string> iconOfProgram(const fs::path& file, std::int32_t index) {
    const std::optional<library::pe::Resources> resources = library::pe::Resources::of(file);
    if (!resources) {
        return std::nullopt;
    }
    Bytes group;
    if (index < 0) {
        group = resources->byId(library::pe::iconGroupType, static_cast<std::uint32_t>(-index));
    } else {
        group = resources->at(library::pe::iconGroupType, static_cast<std::size_t>(index));
    }
    if (group.empty()) {
        group = resources->at(library::pe::iconGroupType, 0);
    }
    const CheckedBytes bytes{group};
    const auto count = bytes.u16(4);
    if (!count) {
        return std::nullopt;
    }
    std::vector<Listed> images;
    for (std::size_t i = 0; i < *count; ++i) {
        const std::size_t at = iconDirSize + (i * groupEntrySize);
        const auto side = bytes.u8(at);
        const auto depth = bytes.u16(at + 6);
        const auto id = bytes.u16(at + 12);
        if (!side || !depth || !id) {
            break;
        }
        const Bytes image = resources->byId(library::pe::iconType, *id);
        if (!image.empty()) {
            images.push_back(Listed{sideOf(*side), *depth, image});
        }
    }
    return bestOf(std::move(images));
}

std::optional<std::string> gameIcon(const library::Game& game,
                                    const std::vector<fs::path>& themes) {
    if (!game.iconSource.empty()) {
        const fs::path source = game.iconSource.is_absolute() || isFile(game.iconSource)
                                    ? game.iconSource
                                    : themeIcon(game.iconSource.string(), themes);
        if (!source.empty() && isFile(source)) {
            if (std::optional<std::string> icon = iconIn(source, game.iconIndex)) {
                return icon;
            }
        }
    }
    if (game.file.empty() || !isFile(game.file)) {
        return std::nullopt;
    }
    if (std::optional<std::string> icon = iconOfProgram(game.file, 0)) {
        return icon;
    }
    const fs::path ico = icoBeside(game.file);
    return ico.empty() ? std::nullopt : iconIn(ico, 0);
}

} // namespace opensu::artwork
