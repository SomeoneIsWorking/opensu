// A GBA link-cable test in multiplayer mode: each player sends its colour (blue added while A is
// held), the parent starts a transfer every frame, and every screen draws what each of the four
// players sent, so a working link shows the same bands on every GBA. Built with `CLOCK_CARTRIDGE`,
// it reads the cartridge's real-time clock every frame and draws it under the bands, with how many
// times this save has booted, kept in the cartridge's flash.
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
    ewram = 0x02000000,
    vram = 0x06000000,
    gpioData = 0x080000c4,
    gpioDirection = 0x080000c6,
    gpioControl = 0x080000c8,
    flash = 0x0e000000,
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

#ifdef CLOCK_CARTRIDGE

// The S-3511 on the cartridge's GPIO: SCK, SIO and CS; a command goes out highest bit first, data
// comes back lowest bit first, and SIO is sampled while SCK is high.
enum {
    sck = 1,
    sio = 2,
    cs = 4,
    readDateTime = 0x65,
    dateTimeBytes = 7,
    hourDigits = 0x3f,
    glyphWidth = 3,
    glyphRows = 5,
    glyphScale = 3,
    glyphPitch = (glyphWidth + 1) * glyphScale,
    clockTop = barTop + 5,
    clockGlyphs = 19,
    dash = 10,
    colon = 11,
    space = 12,
    bootsAfter = 60,
};

// Each row of a glyph in the low three bits, the leftmost highest.
static const uint8_t glyphs[13][glyphRows] = {
    {7, 5, 5, 5, 7}, {2, 2, 2, 2, 2}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
    {0, 0, 7, 0, 0}, {0, 2, 0, 2, 0}, {0, 0, 0, 0, 0},
};

static void clockSend(uint32_t byte) {
    for (int bit = 7; bit >= 0; --bit) {
        const uint16_t data = (uint16_t)(((byte >> bit) & 1) * sio);
        REG16(gpioData) = cs | data;
        REG16(gpioData) = cs | data | sck;
    }
}

static uint32_t clockReceive(void) {
    uint32_t byte = 0;
    for (int bit = 0; bit < 8; ++bit) {
        REG16(gpioData) = cs;
        REG16(gpioData) = cs | sck;
        byte |= (uint32_t)((REG16(gpioData) & sio) != 0) << bit;
    }
    return byte;
}

static void readClock(uint8_t* dateTime) {
    REG16(gpioControl) = 1;
    REG16(gpioDirection) = sck | sio | cs;
    REG16(gpioData) = sck;
    REG16(gpioData) = cs | sck;
    clockSend(readDateTime);
    REG16(gpioDirection) = sck | cs;
    for (int i = 0; i < dateTimeBytes; ++i) {
        dateTime[i] = (uint8_t)clockReceive();
    }
    REG16(gpioData) = sck;
}

static void drawGlyph(uint32_t place, uint32_t glyph) {
    const uint32_t left = 6 + place * glyphPitch;
    for (uint32_t row = 0; row < glyphRows * glyphScale; ++row) {
        volatile uint16_t* line = (volatile uint16_t*)(vram + (clockTop + row) * width * 2);
        const uint32_t bits = glyphs[glyph][row / glyphScale];
        for (uint32_t x = 0; x < glyphWidth * glyphScale; ++x) {
            const uint32_t lit = (bits >> (glyphWidth - 1 - x / glyphScale)) & 1;
            line[left + x] = lit ? white : 0;
        }
    }
}

static void drawClock(const uint8_t* dateTime, uint32_t boots) {
    const uint8_t hour = dateTime[4] & hourDigits;
    const uint8_t digits[6] = {dateTime[0], dateTime[1], dateTime[2],
                               hour,        dateTime[5], dateTime[6]};
    uint32_t place = 0;
    for (uint32_t i = 0; i < 6; ++i) {
        drawGlyph(place++, digits[i] >> 4);
        drawGlyph(place++, digits[i] & 0xf);
        drawGlyph(place++, i < 2 ? dash : (i == 2 ? space : (i < 5 ? colon : space)));
    }
    drawGlyph(clockGlyphs - 1, boots % 10);
}

static void flashCommand(uint8_t command) {
    *(volatile uint8_t*)(flash + 0x5555) = 0xaa;
    *(volatile uint8_t*)(flash + 0x2aaa) = 0x55;
    *(volatile uint8_t*)(flash + 0x5555) = command;
}

static uint32_t bootsSoFar(void) {
    const uint8_t kept = *(volatile uint8_t*)flash;
    return kept == 0xff ? 0 : kept;
}

static void keepBoots(uint32_t boots) {
    flashCommand(0xa0);
    *(volatile uint8_t*)flash = (uint8_t)boots;
}

#endif

int main(void) {
    REG16(dispcnt) = 3 | (1 << 10);
    REG16(rcnt) = 0;
    REG16(siocnt) = multiplayer115200;
    fill(0, 160, 0);

    uint16_t shown[4] = {1, 1, 1, 1};
    uint32_t shownId = 4;
    uint32_t linked = 0;
    uint32_t progress = 0;
#ifdef CLOCK_CARTRIDGE
    const uint32_t boots = bootsSoFar() + 1;
    uint32_t frames = 0;
#endif
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
#ifdef CLOCK_CARTRIDGE
        const uint32_t drawnBarRows = 4;
#else
        const uint32_t drawnBarRows = barRows;
#endif
        for (uint32_t row = barTop; row < barTop + drawnBarRows; ++row) {
            fillRow(row, progress, white, 0);
        }
        REG16(siomltSend) = playerColours[id] ^ ((keys & keyA) ? blue : 0);
        // Kept in main memory too, where a frontend comparing two machines looks.
        REG32(ewram) = progress;
        REG32(ewram + 4) = REG16(siomulti) | ((uint32_t)REG16(siomulti + 2) << 16);
#ifdef CLOCK_CARTRIDGE
        uint8_t dateTime[dateTimeBytes];
        readClock(dateTime);
        drawClock(dateTime, boots);
        for (uint32_t i = 0; i < dateTimeBytes; ++i) {
            *(volatile uint8_t*)(ewram + 8 + i) = dateTime[i];
        }
        if (++frames == bootsAfter) {
            keepBoots(boots);
        }
#endif
        // Started after the read: the registers hold the last transfer only once it is done.
        if (parent && (control & busy) == 0) {
            REG16(siocnt) = control | busy;
        }
    }
}
