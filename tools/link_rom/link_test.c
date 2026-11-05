// A GBA link-cable test in multiplayer mode: each player sends its colour (blue added while A is
// held), the parent starts a transfer every frame, and every screen draws what each of the four
// players sent, so a working link shows the same bands on every GBA.
#include <stdint.h>

#define REG16(address) (*(volatile uint16_t*)(address))
#define REG32(address) (*(volatile uint32_t*)(address))

enum {
    dispcnt = 0x04000000,
    vcount = 0x04000006,
    dma3 = 0x040000d4,
    siomulti = 0x04000120,
    siocnt = 0x04000128,
    siomltSend = 0x0400012a,
    keyinput = 0x04000130,
    rcnt = 0x04000134,
    vram = 0x06000000,
};

enum {
    width = 240,
    idRows = 8,
    bandRows = 32,
    barTop = idRows + 4 * bandRows,
    barRows = 160 - barTop,
};

// SIOCNT: multiplayer mode at 115200 bps; SI low on the parent; SD all ready; ID; start/busy.
enum {
    multiplayer115200 = 0x2003,
    siChild = 1 << 2,
    busy = 1 << 7,
    keyA = 1 << 0,
    blue = 0x7c00,
    white = 0x7fff,
};

static const uint16_t playerColours[4] = {0x001f, 0x03e0, 0x03ff, 0x7c1f};

static void fill(uint32_t row, uint32_t rows, uint16_t colour) {
    volatile uint16_t source = colour;
    REG32(dma3) = (uint32_t)&source;
    REG32(dma3 + 4) = vram + row * width * 2;
    // Enabled, 16-bit units, source fixed.
    REG32(dma3 + 8) = (rows * width) | (0x8100U << 16);
}

static void fillRow(uint32_t row, uint32_t length, uint16_t colour, uint16_t rest) {
    volatile uint16_t* line = (volatile uint16_t*)(vram + row * width * 2);
    for (uint32_t x = 0; x < width; ++x) {
        line[x] = x < length ? colour : rest;
    }
}

static void waitForVblank(void) {
    while (REG16(vcount) >= 160) {
    }
    while (REG16(vcount) < 160) {
    }
}

int main(void) {
    REG16(dispcnt) = 3 | (1 << 10);
    REG16(rcnt) = 0;
    REG16(siocnt) = multiplayer115200;
    fill(0, 160, 0);

    uint16_t shown[4] = {1, 1, 1, 1};
    uint32_t shownId = 4;
    uint32_t linked = 0;
    uint32_t progress = 0;
    for (;;) {
        waitForVblank();
        const uint16_t control = REG16(siocnt);
        const int parent = (control & siChild) == 0;
        const uint32_t id = linked ? (control >> 4) & 3 : (parent ? 0 : 1);
        const uint16_t keys = (uint16_t)~REG16(keyinput);

        if (id != shownId) {
            for (uint32_t row = 0; row < idRows; ++row) {
                fillRow(row, (id + 1) * 60, white, 0);
            }
            shownId = id;
        }
        for (uint32_t player = 0; player < 4; ++player) {
            const uint16_t sent = REG16(siomulti + player * 2);
            if (sent != shown[player]) {
                fill(idRows + player * bandRows, bandRows, sent == 0xffff ? 0 : sent & white);
                shown[player] = sent;
            }
        }
        const uint16_t other = REG16(siomulti + (parent ? 2 : 0));
        if (other != 0xffff && other != 0) {
            linked = 1;
            progress = progress == width ? 0 : progress + 1;
        }
        for (uint32_t row = barTop; row < barTop + barRows; ++row) {
            fillRow(row, progress, white, 0);
        }
        REG16(siomltSend) = playerColours[id] ^ ((keys & keyA) ? blue : 0);
        // Started after the read: the registers hold the last transfer only once it is done.
        if (parent && (control & busy) == 0) {
            REG16(siocnt) = control | busy;
        }
    }
}
