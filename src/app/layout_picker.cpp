#include "layout_picker.hpp"

#include <algorithm>
#include <string>

namespace opensu::app {

void LayoutPicker::open(bool inLibrary) {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    const settings::Settings& values = preferences_.values();
    chooser_.open(values.libraryMode, ui::ChooserValues{}, inLibrary);
    syncValues();
}

void LayoutPicker::syncValues() {
    const settings::Settings& values = preferences_.values();
    ui::ChooserValues& shown = chooser_.values();
    shown.pinned = values.pinLibraryDock;
    shown.iconSize = values.iconSize;
    shown.sort = std::string{library::label(values.view.sort)};
    shown.installedOnly = values.view.installedOnly;
    shown.hiddenOnly = values.view.hiddenOnly;
    shown.source = values.view.source.empty() ? "All sources" : values.view.source;
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == values.view.source) {
            shown.source = choice.label;
        }
    }
}

void LayoutPicker::close() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    chooser_.close();
    if (iconSizeDirty_) {
        iconSizeDirty_ = false;
        preferences_.save();
    }
}

void LayoutPicker::act(gamepad::Button button) {
    ui::ModeChooser& chooser = chooser_;
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (chooser.moveRow(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        actOnRow(button == gamepad::Button::Left ? -1 : 1);
        break;
    case gamepad::Button::A:
        actOnRow(0);
        break;
    case gamepad::Button::B:
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

void LayoutPicker::actOnRow(int step) {
    ui::ModeChooser& chooser = chooser_;
    switch (chooser.row()) {
    case ui::ChooserRow::Cards:
        if (step != 0) {
            if (chooser.move(step)) {
                sounds_.play(audio::Effect::Navigation);
            }
            break;
        }
        chooseMode(chooser.focused());
        close();
        break;
    case ui::ChooserRow::IconSize:
        if (step != 0) {
            stepIconSize(step);
        }
        break;
    case ui::ChooserRow::Pin:
        if (step == 0) {
            togglePin();
        }
        break;
    case ui::ChooserRow::Sort:
        cycleSort(step == 0 ? 1 : step);
        break;
    case ui::ChooserRow::Source:
        cycleSource(step == 0 ? 1 : step);
        break;
    case ui::ChooserRow::Installed:
        if (step == 0) {
            toggleInstalled();
        }
        break;
    case ui::ChooserRow::Hidden:
        if (step == 0) {
            toggleHidden();
        }
        break;
    case ui::ChooserRow::Search:
        if (step == 0) {
            close();
            hooks_.openSearch();
        }
        break;
    }
}

void LayoutPicker::chooseMode(library::LibraryMode mode) {
    settings::Settings& values = preferences_.values();
    if (mode == values.libraryMode) {
        return;
    }
    values.libraryMode = mode;
    hooks_.applyLayout();
    hooks_.reshow(hooks_.focusIndex());
    preferences_.save();
}

void LayoutPicker::togglePin() {
    sounds_.play(audio::Effect::Navigation);
    settings::Settings& values = preferences_.values();
    values.pinLibraryDock = !values.pinLibraryDock;
    hooks_.applyLayout();
    syncValues();
    preferences_.save();
}

void LayoutPicker::stepIconSize(int delta) {
    chooseIconSize(preferences_.values().iconSize + delta);
}

void LayoutPicker::chooseIconSize(int level) {
    settings::Settings& values = preferences_.values();
    const int next = std::clamp(level, settings::minIconSize, settings::maxIconSize);
    if (next == values.iconSize) {
        return;
    }
    sounds_.play(audio::Effect::Navigation);
    values.iconSize = next;
    hooks_.applyLayout();
    iconSizeDirty_ = true;
    syncValues();
}

void LayoutPicker::cycleSort(int delta) {
    sounds_.play(audio::Effect::Navigation);
    settings::Settings& values = preferences_.values();
    const auto count = static_cast<int>(library::allSortKeys.size());
    const int next = (static_cast<int>(values.view.sort) + delta + count) % count;
    values.view.sort = library::allSortKeys[static_cast<std::size_t>(next)];
    viewChanged();
}

void LayoutPicker::cycleSource(int delta) {
    sounds_.play(audio::Effect::Navigation);
    settings::Settings& values = preferences_.values();
    const std::vector<library::SourceChoice> choices = hooks_.sourceChoices();
    const auto at = std::ranges::find(choices, values.view.source, &library::SourceChoice::key);
    const auto count = static_cast<int>(choices.size());
    // A source that is no longer there reads as before the first, so the next is "All sources".
    const int from = at == choices.end() ? -1 : static_cast<int>(at - choices.begin());
    const int next = ((from + delta) % count + count) % count;
    values.view.source = choices[static_cast<std::size_t>(next)].key;
    viewChanged();
}

void LayoutPicker::toggleInstalled() {
    sounds_.play(audio::Effect::Navigation);
    settings::Settings& values = preferences_.values();
    values.view.installedOnly = !values.view.installedOnly;
    viewChanged();
}

void LayoutPicker::toggleHidden() {
    sounds_.play(audio::Effect::Navigation);
    settings::Settings& values = preferences_.values();
    values.view.hiddenOnly = !values.view.hiddenOnly;
    viewChanged();
}

void LayoutPicker::viewChanged() {
    syncValues();
    hooks_.reshow(0);
    preferences_.save();
}

} // namespace opensu::app
