#include "shelf_options.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "settings/settings.hpp"

namespace opensu::app {
namespace {

template <typename T, std::size_t N> std::vector<T> listOf(const std::array<T, N>& all) {
    return {all.begin(), all.end()};
}

} // namespace

ui::SettingsRow ShelfOptions::layout(std::string note) const {
    return ui::makeChoice(std::string{optionLayoutId}, "Library layout", std::move(note),
                          ui::optionsOf(library::allLibraryModes, preferences_.values().libraryMode,
                                        [](library::LibraryMode mode) {
                                            return std::string{library::label(mode)};
                                        }));
}

ui::SettingsRow ShelfOptions::iconSize(std::string note) const {
    return ui::makeSlider(std::string{optionIconSizeId}, "Icon size", std::move(note),
                          ui::SliderRange{.level = preferences_.values().iconSize,
                                          .low = settings::minIconSize,
                                          .high = settings::maxIconSize});
}

ui::SettingsRow ShelfOptions::pin(std::string note) const {
    return ui::makeToggle(std::string{optionPinId}, "Pin navigation bar", std::move(note),
                          preferences_.values().pinLibraryDock);
}

ui::SettingsRow ShelfOptions::sort(std::string note) const {
    return ui::makeChoice(std::string{optionSortId}, "Sort by", std::move(note),
                          ui::optionsOf(library::allSortKeys, preferences_.values().view.sort,
                                        [](library::SortKey key) {
                                            return std::string{library::label(key)};
                                        }));
}

ui::SettingsRow ShelfOptions::source(std::string note) const {
    ui::Options sources;
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == preferences_.values().view.source) {
            sources.chosen = sources.labels.size();
        }
        sources.labels.push_back(choice.label);
    }
    return ui::makeChoice(std::string{optionSourceId}, "Show", std::move(note), std::move(sources));
}

ui::SettingsRow ShelfOptions::installedOnly(std::string note) const {
    return ui::makeToggle(std::string{optionInstalledId}, "Installed games only", std::move(note),
                          preferences_.values().view.installedOnly);
}

ui::SettingsRow ShelfOptions::hiddenOnly(std::string note) const {
    return ui::makeToggle(std::string{optionHiddenId}, "Hidden games only", std::move(note),
                          preferences_.values().view.hiddenOnly);
}

std::vector<ui::SettingsRow> ShelfOptions::rowsFor(library::Section section) const {
    std::vector<ui::SettingsRow> rows;
    if (section == library::Section::Library) {
        rows.push_back(layout());
    }
    rows.push_back(iconSize());
    if (section == library::Section::Library) {
        rows.push_back(pin());
    }
    rows.push_back(sort());
    rows.push_back(source());
    rows.push_back(installedOnly());
    rows.push_back(hiddenOnly());
    return rows;
}

bool ShelfOptions::run(const std::string& id, int step) {
    settings::Settings& values = preferences_.values();
    if (id == optionLayoutId) {
        values.libraryMode =
            ui::stepped(listOf(library::allLibraryModes), values.libraryMode, step);
        hooks_.applyLayout();
        shelfChanged();
    } else if (id == optionPinId) {
        values.pinLibraryDock = !values.pinLibraryDock;
        lookChanged();
    } else if (id == optionSortId) {
        values.view.sort = ui::stepped(listOf(library::allSortKeys), values.view.sort, step);
        shelfChanged();
    } else if (id == optionSourceId) {
        std::vector<std::string> keys;
        for (library::SourceChoice& choice : hooks_.sourceChoices()) {
            keys.push_back(std::move(choice.key));
        }
        if (!keys.empty()) {
            values.view.source = ui::stepped(keys, values.view.source, step);
            shelfChanged();
        }
    } else if (id == optionInstalledId) {
        values.view.installedOnly = !values.view.installedOnly;
        shelfChanged();
    } else if (id == optionHiddenId) {
        values.view.hiddenOnly = !values.view.hiddenOnly;
        shelfChanged();
    } else {
        return false;
    }
    return true;
}

bool ShelfOptions::setLevel(const std::string& id, int level) {
    if (id != optionIconSizeId) {
        return false;
    }
    settings::Settings& values = preferences_.values();
    const int next = std::clamp(level, settings::minIconSize, settings::maxIconSize);
    if (next != values.iconSize) {
        values.iconSize = next;
        lookChanged();
    }
    return true;
}

void ShelfOptions::shelfChanged() {
    hooks_.reshow();
    preferences_.save();
}

void ShelfOptions::lookChanged() {
    hooks_.applyLayout();
    preferences_.save();
}

} // namespace opensu::app
