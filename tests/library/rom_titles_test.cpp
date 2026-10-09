// A ROM's title from its name: No-Intro, Redump, GoodTools, Switch dump and MAME naming.
#include "rom_titles.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using opensu::library::roms::cleanTitle;

struct Row {
    const char* name;
    const char* title;
};

constexpr Row table[] = {
    {"Legend of Zelda, The - Ocarina of Time 3D (USA) (En,Fr,Es)",
     "The Legend of Zelda - Ocarina of Time 3D"},
    {"Legend of Zelda, The - Majora's Mask 3D (USA) (En,Fr,Es) Decrypted",
     "The Legend of Zelda - Majora's Mask 3D"},
    {"Legend of Zelda, The - The Wind Waker (USA)", "The Legend of Zelda - The Wind Waker"},
    {"Kirby & the Amazing Mirror (Europe) (En,Fr,De,Es,It)", "Kirby & the Amazing Mirror"},
    {"Chrono Trigger (U) [!]", "Chrono Trigger"},
    {"Terranigma (E) [!]", "Terranigma"},
    {"2809 - Castlevania - Order of Ecclesia (USA) (En,Fr)", "Castlevania - Order of Ecclesia"},
    {"0881 - Castlevania - Portrait of Ruin (E)(Supremacy)", "Castlevania - Portrait of Ruin"},
    {"Castlevania - Dawn of Sorrow (USA)-patched", "Castlevania - Dawn of Sorrow"},
    {"Donkey Kong Country Returns (USA) (En,Fr,Es) (Rev 1)", "Donkey Kong Country Returns"},
    {"Crash Twinsanity (v1.00)", "Crash Twinsanity"},
    {"Kirby Star Allies[01007E3006DDA000][US][v0]", "Kirby Star Allies"},
    {"Super Mario Bros. Wonder [010015100B514000][v0][US]", "Super Mario Bros. Wonder"},
    {"Mario + Rabbids - Kingdom Battle (USA, Europe) (En,Fr,De,Es,It,Nl)",
     "Mario + Rabbids - Kingdom Battle"},
    {"Street Fighter II: The World Warrior (World 910522)", "Street Fighter II: The World Warrior"},
    {"Alien vs. Predator (USA 940520)", "Alien vs. Predator"},
    {"Tomba! (USA)", "Tomba!"},
    {"Lord of the Rings The - The Return of the King (USA)",
     "Lord of the Rings The - The Return of the King"},
    {"Hunt for Red October, The (USA)", "The Hunt for Red October"},
    {"Aventure de Tintin, L' (France)", "L'Aventure de Tintin"},
    {"Super_Mario_Land (World)", "Super Mario Land"},
    {"Time Crisis 4", "Time Crisis 4"},
    {"Astro Bot PS5 v1.011.000", "Astro Bot PS5 v1.011.000"},
    {"Hades, Lord of the Dead (USA)", "Hades, Lord of the Dead"},
    {"[BIOS] Something (USA)", "[BIOS] Something (USA)"},
    {"(Beta) Foo", "(Beta) Foo"},
};

} // namespace

int main() {
    int failures = 0;
    for (const Row& row : table) {
        const std::string got = cleanTitle(row.name);
        if (got != row.title) {
            std::fprintf(stderr, "FAIL: \"%s\" gave \"%s\", want \"%s\"\n", row.name, got.c_str(),
                         row.title);
            ++failures;
        }
    }
    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::printf("rom_titles: all checks passed\n");
    return 0;
}
