#include "text_fold.hpp"

#include <array>
#include <cstdint>

namespace opensu::library {
namespace {

/// A run of code points `first` to `last` that fold to `to`.
struct Fold {
    char32_t first;
    char32_t last;
    const char* to;
};

constexpr auto folds = std::to_array<Fold>({
    {0xC0, 0xC5, "a"},     {0xC6, 0xC6, "ae"},     {0xC7, 0xC7, "c"},     {0xC8, 0xCB, "e"},
    {0xCC, 0xCF, "i"},     {0xD0, 0xD0, "d"},      {0xD1, 0xD1, "n"},     {0xD2, 0xD6, "o"},
    {0xD8, 0xD8, "o"},     {0xD9, 0xDC, "u"},      {0xDD, 0xDD, "y"},     {0xDE, 0xDE, "th"},
    {0xDF, 0xDF, "ss"},    {0xE0, 0xE5, "a"},      {0xE6, 0xE6, "ae"},    {0xE7, 0xE7, "c"},
    {0xE8, 0xEB, "e"},     {0xEC, 0xEF, "i"},      {0xF0, 0xF0, "d"},     {0xF1, 0xF1, "n"},
    {0xF2, 0xF6, "o"},     {0xF8, 0xF8, "o"},      {0xF9, 0xFC, "u"},     {0xFD, 0xFD, "y"},
    {0xFE, 0xFE, "th"},    {0xFF, 0xFF, "y"},      {0x100, 0x105, "a"},   {0x106, 0x10D, "c"},
    {0x10E, 0x111, "d"},   {0x112, 0x11B, "e"},    {0x11C, 0x123, "g"},   {0x124, 0x127, "h"},
    {0x128, 0x131, "i"},   {0x132, 0x133, "ij"},   {0x134, 0x135, "j"},   {0x136, 0x138, "k"},
    {0x139, 0x142, "l"},   {0x143, 0x14B, "n"},    {0x14C, 0x151, "o"},   {0x152, 0x153, "oe"},
    {0x154, 0x159, "r"},   {0x15A, 0x161, "s"},    {0x162, 0x167, "t"},   {0x168, 0x173, "u"},
    {0x174, 0x175, "w"},   {0x176, 0x178, "y"},    {0x179, 0x17E, "z"},   {0x17F, 0x17F, "s"},
    {0x2018, 0x2019, "'"}, {0x201C, 0x201D, "\""}, {0x2013, 0x2014, "-"}, {0xA0, 0xA0, " "},
    {0x3000, 0x3000, " "}, {0x2122, 0x2122, ""},   {0xAE, 0xAE, ""},      {0xA9, 0xA9, ""},
    {0x300, 0x36F, ""},    {0x2002, 0x200A, " "},
});

/// The code point starting at `at` and the bytes it takes; a byte that is not valid UTF-8 stands
/// for itself.
struct Decoded {
    char32_t point;
    std::size_t length;
};

Decoded decode(std::string_view text, std::size_t at) {
    const auto lead = static_cast<unsigned char>(text[at]);
    std::size_t length = 1;
    char32_t point = lead;
    if (lead >= 0xF0) {
        length = 4;
        point = lead & 0x07U;
    } else if (lead >= 0xE0) {
        length = 3;
        point = lead & 0x0FU;
    } else if (lead >= 0xC0) {
        length = 2;
        point = lead & 0x1FU;
    }
    if (length == 1 || at + length > text.size()) {
        return {lead, 1};
    }
    for (std::size_t i = 1; i < length; ++i) {
        const auto next = static_cast<unsigned char>(text[at + i]);
        if ((next & 0xC0U) != 0x80U) {
            return {lead, 1};
        }
        point = (point << 6U) | (next & 0x3FU);
    }
    return {point, length};
}

const Fold* foldOf(char32_t point) {
    for (const Fold& fold : folds) {
        if (point >= fold.first && point <= fold.last) {
            return &fold;
        }
    }
    return nullptr;
}

} // namespace

std::string foldText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool pendingSpace = false;
    const auto put = [&](std::string_view piece) {
        for (const char c : piece) {
            if (c == ' ') {
                pendingSpace = !out.empty();
                continue;
            }
            if (pendingSpace) {
                out += ' ';
                pendingSpace = false;
            }
            out += c;
        }
    };
    for (std::size_t at = 0; at < text.size();) {
        const auto [point, length] = decode(text, at);
        if (point < 0x80) {
            char c = static_cast<char>(point);
            if (c == '\t' || c == '\n' || c == '\r') {
                c = ' ';
            } else if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c + ('a' - 'A'));
            }
            put(std::string_view{&c, 1});
        } else if (const Fold* fold = foldOf(point)) {
            put(fold->to);
        } else {
            put(text.substr(at, length));
        }
        at += length;
    }
    return out;
}

} // namespace opensu::library
