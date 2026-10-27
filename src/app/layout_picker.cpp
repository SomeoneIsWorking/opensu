#include "layout_picker.hpp"

#include <algorithm>
#include <memory>

#include "layout_picker_screen.hpp"

namespace opensu::app {

std::string layoutCardId(library::LibraryMode mode) {
    return "options.card." + std::string{library::key(mode)};
}

const gpu2d::ui::Screen* LayoutPicker::panel() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const LayoutPickerScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool LayoutPicker::isOpen() const {
    return panel() != nullptr;
}

void LayoutPicker::open(bool inLibrary) {
    if (isOpen()) {
        return;
    }
    inLibrary_ = inLibrary;
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<LayoutPickerScreen>(*this));
}

void LayoutPicker::close() {
    const gpu2d::ui::Screen* open = panel();
    if (open == nullptr) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*open);
}

std::vector<ui::SettingsRow> LayoutPicker::rows() const {
    const settings::Settings& values = preferences_.values();
    std::vector<ui::SettingsRow> rows;
    if (inLibrary_) {
        rows.push_back(ui::makeSlider(std::string{optionIconSizeId}, "Icon size", {},
                                      ui::SliderRange{.level = values.iconSize,
                                                      .low = settings::minIconSize,
                                                      .high = settings::maxIconSize}));
        rows.push_back(ui::makeToggle(std::string{optionPinId}, "Pin navigation bar on Library", {},
                                      values.pinLibraryDock));
    }
    ui::Options sorts;
    for (const library::SortKey sort : library::allSortKeys) {
        if (sort == values.view.sort) {
            sorts.chosen = sorts.labels.size();
        }
        sorts.labels.emplace_back(library::label(sort));
    }
    rows.push_back(ui::makeChoice(std::string{optionSortId}, "Sort by", {}, std::move(sorts)));
    ui::Options sources;
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == values.view.source) {
            sources.chosen = sources.labels.size();
        }
        sources.labels.push_back(choice.label);
    }
    rows.push_back(ui::makeChoice(std::string{optionSourceId}, "Show", {}, std::move(sources)));
    rows.push_back(ui::makeToggle(std::string{optionInstalledId}, "Installed games only", {},
                                  values.view.installedOnly));
    rows.push_back(ui::makeToggle(std::string{optionHiddenId}, "Hidden games only", {},
                                  values.view.hiddenOnly));
    rows.push_back(ui::makePage(std::string{optionSearchId}, "Search", {}, {}));
    rows.push_back(ui::makePage(std::string{optionSettingsId}, "Settings", {}, {}));
    return rows;
}

void LayoutPicker::chooseMode(library::LibraryMode mode) {
    settings::Settings& values = preferences_.values();
    if (mode != values.libraryMode) {
        values.libraryMode = mode;
        hooks_.applyLayout();
        hooks_.reshow(hooks_.focusIndex());
        preferences_.save();
    }
    close();
}

void LayoutPicker::run(const std::string& id, int step) {
    settings::Settings& values = preferences_.values();
    if (id == optionPinId) {
        values.pinLibraryDock = !values.pinLibraryDock;
        hooks_.applyLayout();
        preferences_.save();
    } else if (id == optionSortId) {
        const auto count = static_cast<int>(library::allSortKeys.size());
        const int next = ((static_cast<int>(values.view.sort) + step) % count + count) % count;
        values.view.sort = library::allSortKeys[static_cast<std::size_t>(next)];
        viewChanged();
    } else if (id == optionSourceId) {
        const std::vector<library::SourceChoice> choices = hooks_.sourceChoices();
        if (choices.empty()) {
            return;
        }
        const auto at = std::ranges::find(choices, values.view.source, &library::SourceChoice::key);
        const auto count = static_cast<int>(choices.size());
        const int from = at == choices.end() ? 0 : static_cast<int>(at - choices.begin());
        const int next = ((from + step) % count + count) % count;
        values.view.source = choices[static_cast<std::size_t>(next)].key;
        viewChanged();
    } else if (id == optionInstalledId) {
        values.view.installedOnly = !values.view.installedOnly;
        viewChanged();
    } else if (id == optionHiddenId) {
        values.view.hiddenOnly = !values.view.hiddenOnly;
        viewChanged();
    }
}

void LayoutPicker::setLevel(const std::string& id, int level) {
    settings::Settings& values = preferences_.values();
    const int next = std::clamp(level, settings::minIconSize, settings::maxIconSize);
    if (id != optionIconSizeId || next == values.iconSize) {
        return;
    }
    values.iconSize = next;
    hooks_.applyLayout();
    preferences_.save();
}

void LayoutPicker::openPage(const std::string& id) {
    if (id == optionSearchId) {
        close();
        hooks_.openSearch();
    } else if (id == optionSettingsId) {
        close();
        hooks_.openSettings();
    }
}

void LayoutPicker::viewChanged() {
    hooks_.reshow(0);
    preferences_.save();
}

} // namespace opensu::app
