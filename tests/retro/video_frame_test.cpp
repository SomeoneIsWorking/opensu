// The core's three pixel formats to RGBA, padded rows, and duplicated frames.
#include "video_frame.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

#include "check.hpp"

namespace {

using opensu::retro::VideoFrame;
using opensu::test::expect;

void expectPixel(std::span<const std::uint8_t> rgba, std::size_t index, std::uint8_t r,
                 std::uint8_t g, std::uint8_t b, const char* what) {
    expect(rgba[index * 4] == r && rgba[index * 4 + 1] == g && rgba[index * 4 + 2] == b &&
               rgba[index * 4 + 3] == 0xFF,
           what);
}

void xrgb8888() {
    // Two pixels per row, rows padded to 12 bytes.
    const std::array<std::uint32_t, 6> rows{0x00FF8040, 0xAA102030, 0xDEADBEEF,
                                            0x00000000, 0x00FFFFFF, 0x12345678};
    VideoFrame frame;
    frame.take(rows.data(), 2, 2, 12, RETRO_PIXEL_FORMAT_XRGB8888);
    expect(frame.width() == 2 && frame.height() == 2, "the frame keeps its size");
    expectPixel(frame.rgba(), 0, 0xFF, 0x80, 0x40, "XRGB8888 red, green, blue");
    expectPixel(frame.rgba(), 1, 0x10, 0x20, 0x30, "XRGB8888 ignores the top byte");
    expectPixel(frame.rgba(), 2, 0x00, 0x00, 0x00, "the second row starts after the pitch");
    expectPixel(frame.rgba(), 3, 0xFF, 0xFF, 0xFF, "XRGB8888 white");
}

void rgb565() {
    const std::array<std::uint16_t, 4> pixels{0xF800, 0x07E0, 0x001F, 0x8410};
    VideoFrame frame;
    frame.take(pixels.data(), 4, 1, 8, RETRO_PIXEL_FORMAT_RGB565);
    expectPixel(frame.rgba(), 0, 0xFF, 0x00, 0x00, "RGB565 red is full");
    expectPixel(frame.rgba(), 1, 0x00, 0xFF, 0x00, "RGB565 green is full");
    expectPixel(frame.rgba(), 2, 0x00, 0x00, 0xFF, "RGB565 blue is full");
    expectPixel(frame.rgba(), 3, 0x84, 0x82, 0x84, "RGB565 mid grey widens its high bits");
}

void xrgb1555() {
    const std::array<std::uint16_t, 4> pixels{0x7C00, 0x03E0, 0x001F, 0xFFFF};
    VideoFrame frame;
    frame.take(pixels.data(), 4, 1, 8, RETRO_PIXEL_FORMAT_0RGB1555);
    expectPixel(frame.rgba(), 0, 0xFF, 0x00, 0x00, "0RGB1555 red is full");
    expectPixel(frame.rgba(), 1, 0x00, 0xFF, 0x00, "0RGB1555 green is full");
    expectPixel(frame.rgba(), 2, 0x00, 0x00, 0xFF, "0RGB1555 blue is full");
    expectPixel(frame.rgba(), 3, 0xFF, 0xFF, 0xFF, "0RGB1555 ignores the top bit");
}

void duplicates() {
    const std::array<std::uint16_t, 1> pixel{0xF800};
    VideoFrame frame;
    expect(!frame.takeFresh(), "no frame is fresh before the first");
    frame.take(pixel.data(), 1, 1, 2, RETRO_PIXEL_FORMAT_RGB565);
    expect(frame.takeFresh(), "a frame is fresh once taken");
    expect(!frame.takeFresh(), "and only once");
    frame.take(nullptr, 1, 1, 2, RETRO_PIXEL_FORMAT_RGB565);
    expect(!frame.takeFresh(), "a duplicate is not fresh");
    expectPixel(frame.rgba(), 0, 0xFF, 0x00, 0x00, "a duplicate keeps the last frame");
}

} // namespace

int main() {
    xrgb8888();
    rgb565();
    xrgb1555();
    duplicates();
    std::printf("video_frame: all checks passed\n");
    return 0;
}
