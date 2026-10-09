// Matching ROM names against a libretro-thumbnails listing.
#include "libretro_index.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::artwork::bestMatch;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::vector<std::string> releaseIndex() {
    return std::vector<std::string>{
        "Legend of Zelda, The - The Wind Waker (Europe) (En,Fr,De,Es,It)",
        "Legend of Zelda, The - The Wind Waker (Japan)",
        "Legend of Zelda, The - The Wind Waker (USA)",
        "Legend of Zelda, The - The Wind Waker (USA) (Demo)",
        "Super Mario Sunshine (Europe) (En,Fr,De,Es,It)",
        "Super Mario Sunshine (USA)",
        "Super Mario Sunshine (USA) (Rev 1)",
        "Mario & Luigi - Superstar Saga (USA)",
        "Xenoblade Chronicles (Europe, Australia)",
        "Pikmin (Beta)",
        "Castlevania - Portrait of Ruin (Europe) (En,Fr,De,Es,It)",
        "Castlevania - Portrait of Ruin (USA)",
        "Kirby & The Amazing Mirror (Europe) (En,Fr,De,Es,It)",
        "Kirby & The Amazing Mirror (USA)",
    };
}

void testParseIndex() {
    const std::vector<std::string> names = opensu::artwork::parseIndex(
        R"(<a href="?C=N;O=D">Name</a> <a href="Super%20Mario%20Sunshine%20(USA).png">Super)"
        R"( Mario</a> <a href="Mario%20_%20Luigi%20-%20Superstar%20Saga%20(USA).png">x</a>)"
        R"( <a href="../">Parent</a>)");
    expect(names.size() == 2 && names[0] == "Super Mario Sunshine (USA)" &&
               names[1] == "Mario _ Luigi - Superstar Saga (USA)",
           "png links are listed, decoded and without the extension");
}

void testMatches() {
    expect(bestMatch("Legend of Zelda, The - The Wind Waker (USA)", releaseIndex()) ==
               "Legend of Zelda, The - The Wind Waker (USA)",
           "the same name is taken as it is");
    expect(bestMatch("Legend of Zelda, The - The Wind Waker (Japan) (Rev 2)", releaseIndex()) ==
               "Legend of Zelda, The - The Wind Waker (Japan)",
           "the ROM's own region wins");
    expect(bestMatch("The Legend of Zelda The Wind Waker", releaseIndex()) ==
               "Legend of Zelda, The - The Wind Waker (USA)",
           "a plain folder name matches its title, USA first, never the demo");
    expect(bestMatch("Super Mario Sunshine", releaseIndex()) == "Super Mario Sunshine (USA)",
           "the entry with the fewest extra tags wins within a region");
    expect(bestMatch("Mario & Luigi - Superstar Saga (USA)", releaseIndex()) ==
               "Mario & Luigi - Superstar Saga (USA)",
           "an ampersand matches the listing's own name");
    expect(bestMatch("Xenoblade Chronicles", releaseIndex()) ==
               "Xenoblade Chronicles (Europe, Australia)",
           "any region when it is the only one");
    expect(!bestMatch("Pikmin", releaseIndex()), "a beta is not taken for a release");
    expect(bestMatch("0881 - Castlevania - Portrait of Ruin (E)(Supremacy)", releaseIndex()) ==
               "Castlevania - Portrait of Ruin (Europe) (En,Fr,De,Es,It)",
           "a release number goes and GoodTools' (E) is Europe");
    expect(bestMatch("Kirby _ the Amazing Mirror (Europe) (En,Fr,De,Es,It)", releaseIndex()) ==
               "Kirby & The Amazing Mirror (Europe) (En,Fr,De,Es,It)",
           "an underscore standing for an ampersand still matches");
    expect(!bestMatch("Xenoblade Chronicles 2", releaseIndex()), "a different title is no match");
}

void testNames() {
    expect(opensu::artwork::thumbnailName("Mario & Luigi: Saga?") == "Mario _ Luigi_ Saga_",
           "libretro's file name characters");
    expect(opensu::artwork::encodeSegment("Sony - PlayStation 2") == "Sony%20-%20PlayStation%202",
           "spaces are encoded");
    expect(opensu::artwork::encodeSegment("Ico (Europe, Australia) #1") ==
               "Ico%20(Europe,%20Australia)%20%231",
           "parentheses and commas stay as the listing writes them");
}

} // namespace

int main() {
    testParseIndex();
    testMatches();
    testNames();
    std::printf("libretro_index: all checks passed\n");
    return 0;
}
