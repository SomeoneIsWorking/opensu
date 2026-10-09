// The dock's sections cycling with wrap, and the spellings of sections and Library modes.
#include "library/sections.hpp"

#include <cstdio>

#include "ui/check.hpp"

namespace {

using iideck::library::LibraryMode;
using iideck::library::Section;
using iideck::library::Sections;
using iideck::test::expect;

void cyclingWraps() {
    Sections sections;
    expect(sections.active() == Section::Home, "the first section is Home");
    expect(sections.cycle(1) == Section::Library && sections.active() == Section::Library,
           "R1 from Home reaches Library");
    expect(sections.cycle(1) == Section::Home, "R1 from the last section wraps to the first");
    expect(sections.cycle(-1) == Section::Library, "L1 from the first section wraps to the last");
    expect(sections.cycle(-1) == Section::Home, "L1 moves back one");
    expect(sections.cycle(2) == Section::Home && sections.cycle(-3) == Section::Library,
           "a longer step wraps as often as it needs to");
    sections.select(Section::Home);
    expect(sections.active() == Section::Home, "a tap selects a section directly");
}

void stepsReachASection() {
    for (const Section from : iideck::library::allSections) {
        for (const Section to : iideck::library::allSections) {
            Sections sections;
            sections.select(from);
            expect(sections.cycle(Sections::stepsBetween(from, to)) == to,
                   "the steps between two sections cycle from one to the other");
        }
    }
    expect(Sections::stepsBetween(Section::Library, Section::Library) == 0,
           "a section is no steps from itself");
}

void spellings() {
    expect(iideck::library::key(Section::Home) == "home" &&
               iideck::library::key(Section::Library) == "library",
           "sections have control channel names");
    for (const LibraryMode mode : iideck::library::allLibraryModes) {
        expect(iideck::library::libraryModeOf(iideck::library::key(mode)) == mode,
               "a mode's key names it back");
    }
    expect(!iideck::library::libraryModeOf("list") && !iideck::library::libraryModeOf(""),
           "iiSU's List layout has no picker, so it is no mode here");
    expect(iideck::library::label(LibraryMode::Xmb) == "XMB" &&
               iideck::library::label(LibraryMode::Carousel) == "Carousel" &&
               iideck::library::label(LibraryMode::Standard) == "Standard",
           "the card names are iiSU's");
}

} // namespace

int main() {
    cyclingWraps();
    stepsReachASection();
    spellings();
    std::printf("sections: all checks passed\n");
    return 0;
}
