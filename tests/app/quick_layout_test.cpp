// What Quick settings holds: the default tabs until the player changes them, items moved within
// and across categories, categories and tabs moved, removed and renamed, a pin from Settings added
// and taken out, and the layout kept across a restart in the settings file.
#include <cstdio>
#include <string>
#include <vector>

#include "quick_layout.hpp"
#include "settings/settings.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using app::ItemPlace;
using test::expect;

struct Rig {
    Rig()
        : scratch{OPENSU_TEST_SCRATCH, "quick-layout"},
          preferences{settings::Store{scratch.path() / "settings.json"}, [](const std::string&) {
                      }} {
    }

    [[nodiscard]] settings::Settings onDisk() const {
        return settings::Store{scratch.path() / "settings.json"}.load();
    }
    [[nodiscard]] std::vector<std::string> rows(std::size_t tab, std::size_t category) const {
        std::vector<std::string> ids;
        for (const settings::QuickPin& pin : editor.layout().tabs[tab].categories[category].items) {
            ids.push_back(pin.row);
        }
        return ids;
    }

    test::ScratchDir scratch;
    app::Preferences preferences;
    app::QuickLayoutEditor editor{preferences};
};

void defaultsAreTheAgreedTabs() {
    Rig rig;
    const settings::QuickLayout layout = rig.editor.layout();
    expect(layout.tabs.size() == 3 && layout.tabs[0].icon == "sliders" &&
               layout.tabs[1].icon == "display" && layout.tabs[2].icon == "gamepad",
           "System, View and Device tabs, each an icon");
    expect(rig.rows(0, 0) == std::vector<std::string>{"system.volume", "system.mute"},
           "System starts with Sound: Volume and Mute");
    expect(!rig.onDisk().quickSettings, "unset until the player changes it");
}

void itemsMoveAcrossCategories() {
    Rig rig;
    ItemPlace at = rig.editor.moveItem({0, 0, 0}, 1);
    expect(at == ItemPlace{0, 0, 1} && rig.rows(0, 0).back() == "system.volume",
           "Down moves Volume under Mute");
    at = rig.editor.moveItem(at, 1);
    expect(at == ItemPlace{0, 1, 0} && rig.rows(0, 1).front() == "system.volume",
           "past its category's end it heads the next category");
    at = rig.editor.moveItem(at, -1);
    expect(at == ItemPlace{0, 0, 1}, "Up at a category's top ends the one above");
    const ItemPlace first = rig.editor.moveItem({0, 0, 0}, -1);
    expect(first == ItemPlace{0, 0, 0}, "the tab's first item goes no higher");
    expect(rig.onDisk().quickSettings.has_value(), "a move is kept");
}

void categoriesAndTabsChange() {
    Rig rig;
    expect(rig.editor.moveCategory(0, 0, 1) == 1 &&
               rig.editor.layout().tabs[0].categories[1].name == "Sound",
           "a category moves down");
    rig.editor.renameCategory(0, 1, "Audio");
    expect(rig.editor.layout().tabs[0].categories[1].name == "Audio", "and is renamed");
    const std::size_t added = rig.editor.addCategory(0, {});
    expect(rig.editor.layout().tabs[0].categories[added].name == "New category" &&
               rig.editor.layout().tabs[0].categories[added].items.empty(),
           "an added category is empty, with a name to change");
    rig.editor.removeCategory(0, added);
    const std::size_t tab = rig.editor.addTab(app::QuickIcon::Star);
    expect(rig.editor.layout().tabs.size() == 4 && rig.editor.layout().tabs[tab].icon == "star",
           "a tab is added last with its icon");
    rig.editor.setIcon(tab, app::QuickIcon::Moon);
    expect(rig.editor.moveTab(tab, -1) == 2 && rig.editor.layout().tabs[2].icon == "moon",
           "its icon changes and it moves left");
    rig.editor.removeTab(2);
    expect(rig.editor.layout().tabs.size() == 3, "and it is removed");
    rig.editor.removeItem({0, 1, 0});
    expect(rig.rows(0, 1).size() == 1, "an item is removed");
}

void aPinFromSettingsComesAndGoes() {
    Rig rig;
    const settings::QuickPin night{"appearance", "system.nightLight"};
    expect(rig.editor.pinned(night), "Night light is in by default");
    expect(!rig.editor.togglePin(night) && !rig.editor.pinned(night), "taken out");
    expect(rig.editor.togglePin(night) && rig.editor.pinned(night), "and added again");
    const settings::QuickLayout layout = rig.editor.layout();
    expect(layout.tabs[0].categories.back().items.back() == night,
           "to the end of the first tab's last category");
    rig.editor.addItem(0, 0, {"audio", "system.volume"});
    expect(rig.rows(0, 0).size() == 2, "an item already in a category is not added twice");
}

void theLayoutIsKept() {
    Rig rig;
    rig.editor.addTab(app::QuickIcon::Network);
    rig.editor.renameCategory(3, 0, "Online");
    rig.editor.addItem(3, 0, {"lan", "lan.name", "Name on the network"});
    const settings::Settings kept = rig.onDisk();
    expect(kept.quickSettings == rig.editor.layout(),
           "tabs, icons, names and items read back as written");
    expect(kept.quickSettings->tabs[3].categories[0].items[0].label == "Name on the network",
           "with the label an item shows where this machine lacks its row");
}

} // namespace

int main() try {
    defaultsAreTheAgreedTabs();
    itemsMoveAcrossCategories();
    categoriesAndTabsChange();
    aPinFromSettingsComesAndGoes();
    theLayoutIsKept();
    std::printf("quick layout: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
