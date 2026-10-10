// The session dialog: focus between Accept and Cancel, the progress line replacing the buttons, and
// the layout the pointer hit-tests.
#include "session_dialog.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;

SessionDialogText text() {
    return SessionDialogText{"Install session mode", {"One.", "Two."}, "Accept", "Cancel"};
}

void focusMovesBetweenTheButtons() {
    SessionDialog dialog;
    expect(!dialog.isOpen(), "closed at first");
    dialog.open(text());
    expect(dialog.isOpen() && dialog.focus() == dialogAccept, "opens on Accept");
    expect(!dialog.move(Direction::Left), "there is nothing left of Accept");
    expect(dialog.move(Direction::Right) && dialog.focus() == dialogCancel, "Right is Cancel");
    expect(!dialog.move(Direction::Down), "and stays there");
    expect(dialog.move(Direction::Up) && dialog.focus() == dialogAccept, "Up goes back");
    expect(dialog.focusButton(dialogCancel) && !dialog.focusButton(dialogCancel) &&
               !dialog.focusButton(7),
           "a pointer focuses a button, once, and no other");
    dialog.open(text());
    expect(dialog.focus() == dialogAccept, "reopening focuses Accept again");
}

void progressReplacesTheButtons() {
    SessionDialog dialog;
    dialog.open(text());
    dialog.showProgress("Installing...");
    expect(dialog.busy() && dialog.progress().value_or("") == "Installing...", "the line is shown");
    dialog.close();
    expect(!dialog.isOpen() && !dialog.busy(), "closing ends it");
}

void layoutHitTestsTheButtons() {
    const SessionDialogLayout layout = layoutSessionDialog(
        PanelFrame{1280.0f, 720.0f, 1.0f}, SessionDialogMetrics{30.0f, 20.0f, 120.0f}, true);
    expect(layout.card.x >= 0.0f && layout.card.right() <= 1280.0f && layout.card.y >= 0.0f &&
               layout.card.bottom() <= 720.0f,
           "the card fits the frame");
    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
        const auto hit = layout.buttonAt(layout.buttons[i].centreX(), layout.buttons[i].centreY());
        expect(hit && *hit == i, "a button's centre hits that button");
        expect(layout.buttons[i].x >= layout.card.x &&
                   layout.buttons[i].right() <= layout.card.right() &&
                   layout.buttons[i].bottom() <= layout.card.bottom(),
               "and it is inside the card");
    }
    expect(layout.buttons[dialogAccept].right() <= layout.buttons[dialogCancel].x,
           "Accept is left of Cancel");
    expect(!layout.buttonAt(layout.card.x + 1.0f, layout.card.y + 1.0f), "the title hits nothing");
    const SessionDialogLayout running = layoutSessionDialog(
        PanelFrame{1280.0f, 720.0f, 1.0f}, SessionDialogMetrics{30.0f, 20.0f, 120.0f}, false);
    expect(!running.buttonAt(640.0f, 360.0f) && running.card.height < layout.card.height,
           "while it runs there are no buttons to hit");
    const SessionDialogLayout narrow = layoutSessionDialog(
        PanelFrame{400.0f, 300.0f, 1.0f}, SessionDialogMetrics{30.0f, 20.0f, 120.0f}, true);
    expect(narrow.card.width <= 400.0f && dialogBodyWidth(400.0f, 1.0f) < 400.0f,
           "the card never exceeds a narrow frame");
}

} // namespace

int main() {
    focusMovesBetweenTheButtons();
    progressReplacesTheButtons();
    layoutHitTestsTheButtons();
    std::printf("session_dialog: all checks passed\n");
    return 0;
}
