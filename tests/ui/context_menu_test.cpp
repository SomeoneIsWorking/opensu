// The context menu: what each tile offers, focus movement and the layout the pointer hits.
#include "context_menu.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using namespace opensu::library;
using opensu::test::expect;

std::vector<ContextAction> actions(const ShelfItem& item, bool hidden) {
    std::vector<ContextAction> out;
    for (const ContextItem& entry : contextItemsFor(item, hidden)) {
        out.push_back(entry.action);
    }
    return out;
}

void gamesOfferLaunchOrInstallDetailsAndHide() {
    Game installed;
    installed.installed = true;
    Game missing;
    using A = ContextAction;
    expect((actions(installed, false) == std::vector<A>{A::Launch, A::Details, A::Hide}),
           "an installed game launches, shows details and hides");
    expect((actions(missing, false) == std::vector<A>{A::Install, A::Details, A::Hide}),
           "a missing game installs");
    expect((actions(installed, true) == std::vector<A>{A::Launch, A::Details, A::Unhide}),
           "a hidden game unhides");
}

void foldersAndStoresOfferTheirOwn() {
    using A = ContextAction;
    expect((actions(Console{}, false) == std::vector<A>{A::Open, A::Refresh}), "a console opens");
    expect((actions(Launcher{Source::Gog, 0, false, false}, false) ==
            std::vector<A>{A::Open, A::SignIn, A::Refresh}),
           "a GOG launcher without a sign-in offers one");
    expect((actions(Launcher{Source::Gog, 3, true, false}, false) ==
            std::vector<A>{A::Open, A::Refresh}),
           "a ready store does not");
    expect((actions(Launcher{Source::Steam, 0, false, false}, false) ==
            std::vector<A>{A::Open, A::Refresh}),
           "Steam has no sign-in of its own");
}

void focusStopsAtTheEnds() {
    ContextMenu menu;
    menu.open("Hades", contextItemsFor(Game{}, false));
    expect(menu.isOpen() && menu.focus() == 0 && menu.title() == "Hades", "opens on the first");
    expect(!menu.move(-1), "no item above the first");
    expect(menu.move(1) && menu.move(1) && !menu.move(1) && menu.focus() == 2, "stops at the last");
    expect(menu.selected() == ContextAction::Hide, "the focused action is selected");
    expect(menu.focusItem(0) && !menu.focusItem(0) && !menu.focusItem(9), "focus by index");
    menu.close();
    expect(!menu.isOpen(), "closes");
}

void layoutHitTests() {
    const Rect frame{0.0f, 0.0f, 1280.0f, 720.0f};
    const ContextLayout layout = layoutContextMenu(frame, 1.0f, 3);
    expect(layout.rows.size() == 3, "a row per item");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto hit = layout.itemAt(layout.rows[i].centreX(), layout.rows[i].y + 2.0f);
        expect(hit && *hit == i, "a row hits itself");
        expect(layout.contains(layout.rows[i].centreX(), layout.rows[i].y + 2.0f), "on the card");
    }
    expect(!layout.contains(1.0f, 1.0f) && !layout.itemAt(1.0f, 1.0f), "outside is the backdrop");
    expect(layout.card.x >= 0.0f && layout.card.right() <= frame.width, "the card fits");
}

} // namespace

int main() {
    gamesOfferLaunchOrInstallDetailsAndHide();
    foldersAndStoresOfferTheirOwn();
    focusStopsAtTheEnds();
    layoutHitTests();
    std::printf("context_menu: all checks passed\n");
    return 0;
}
