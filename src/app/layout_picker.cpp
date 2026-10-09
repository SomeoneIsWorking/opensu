#include "layout_picker.hpp"

#include <string>

#include "lucent/log.h"

namespace opensu::app {

void LayoutPicker::open() {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    shell_.modeChooser().open(preferences_.libraryMode, preferences_.pinLibraryDock);
}

void LayoutPicker::act(gamepad::Button button) {
    ui::ModeChooser& chooser = shell_.modeChooser();
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        if (chooser.moveRow(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (chooser.row() == ui::ChooserRow::Cards &&
            chooser.move(button == gamepad::Button::Left ? -1 : 1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        if (chooser.row() == ui::ChooserRow::Pin) {
            togglePin();
            break;
        }
        chooseMode(chooser.focused());
        // input-sound.md 3.4 Close: the player dismisses a panel.
        sounds_.play(audio::Effect::Close);
        chooser.close();
        break;
    case gamepad::Button::B:
        sounds_.play(audio::Effect::Close);
        chooser.close();
        break;
    default:
        break;
    }
}

void LayoutPicker::chooseMode(library::LibraryMode mode) {
    if (mode == preferences_.libraryMode) {
        return;
    }
    preferences_.libraryMode = mode;
    shell_.setLibraryMode(mode);
    reshow_();
    save();
}

void LayoutPicker::togglePin() {
    sounds_.play(audio::Effect::Navigation);
    preferences_.pinLibraryDock = shell_.modeChooser().togglePin();
    shell_.setPinLibraryDock(preferences_.pinLibraryDock);
    save();
}

void LayoutPicker::save() {
    std::string error;
    if (!store_.save(preferences_, error)) {
        lucent::error("settings", "{}", error);
        shell_.setToast("cannot keep the layout: " + error, true);
    }
}

} // namespace opensu::app
