// Installing session mode: the question on the interface toolkit, Cancel, the password in a masked
// field typed on the keyboard (drawn or physical), the card while it installs, a wrong password and
// the retry, a failure, and the password gone from the field afterwards.
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "host_fakes.hpp"
#include "keyboard_pad.hpp"
#include "session_install_controller.hpp"
#include "support/fake_interface_text.hpp"
#include "ui/check.hpp"
#include "ui/entry_screen.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using gpu2d::ui::Action;
using host::InstallResult;
using test::expect;

struct Rig {
    Rig()
        : controller{ui.navigator, pad, session, sounds,
                     [this](const std::string& text, bool error) {
                         said.push_back((error ? "error: " : "") + text);
                     }} {
        ui.navigator.setKeyboard([this] {
            pad.open();
        });
    }

    /// Runs the loop until the install has finished.
    void finishInstall() {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (controller.installing() && std::chrono::steady_clock::now() < until) {
            controller.service();
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        expect(!controller.installing(), "the install finished");
    }
    /// A frame of the loop: the stack laid out, the keyboard following the focus.
    void frame() {
        ui.frame(1.0);
        pad.follow();
    }
    /// The id of the top screen, or empty with none open.
    [[nodiscard]] std::string top() const {
        const gpu2d::ui::Screen* screen = ui.navigator.top();
        return screen == nullptr ? std::string{} : screen->id();
    }
    /// The password field, or null when it is not shown.
    [[nodiscard]] const gpu2d::ui::TextField* field() {
        return dynamic_cast<const gpu2d::ui::TextField*>(
            ui.navigator.find(ui::EntryScreen::fieldId));
    }
    [[nodiscard]] std::string typed() {
        const gpu2d::ui::TextField* shown = field();
        return shown == nullptr ? std::string{} : shown->text();
    }
    void press(Action action) {
        ui.navigator.act(action);
        frame();
    }
    void button(Button pressed) {
        pad.act(pressed);
        frame();
    }
    /// Types `text` on the physical keyboard and presses Enter.
    void typeAndEnter(const std::string& text) {
        ui.navigator.text(text);
        press(Action::Submit);
    }
    /// Asks and answers Install.
    void accept() {
        controller.begin();
        frame();
        press(Action::Accept);
    }
    void focusKey(ui::KeyKind kind, char character = '\0') {
        const auto keys = ui::gridKeys();
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (keys[i].kind == kind &&
                (kind != ui::KeyKind::Character || keys[i].character == character)) {
                keyboard.focusKey(i);
                return;
            }
        }
    }

    test::InterfaceRig ui;
    ui::OnScreenKeyboard keyboard;
    audio::SdlSoundPlayer sounds;
    app::KeyboardPad pad{keyboard, ui.navigator, sounds};
    test::FakeSessionMode session;
    std::vector<std::string> said;
    app::SessionInstallController controller;
};

void aBlockedInstallSaysWhyAndOpensNothing() {
    Rig rig;
    rig.session.blocker = "openSU is not installed here";
    rig.controller.begin();
    expect(rig.top().empty() &&
               rig.said == std::vector<std::string>{"error: openSU is not installed here"},
           "the reason is said and nothing asks");
}

void theDialogExplainsAndCancelAborts() {
    Rig rig;
    rig.controller.begin();
    rig.frame();
    const auto* question = dynamic_cast<const gpu2d::ui::Dialog*>(rig.ui.navigator.top());
    expect(question != nullptr && !rig.controller.typing() &&
               rig.ui.navigator.focused() == "dialog/1",
           "the explanation asks first, on Install");
    expect(question->title() == "Install session mode", "it is titled");
    for (const char* mention : {"login session", "Gamescope", "Switch to desktop",
                                "/usr/local/libexec/opensu-session-select", "/etc/sudoers.d",
                                "/usr/local/share/wayland-sessions", "administrator password"}) {
        expect(question->body().find(mention) != std::string::npos, mention);
    }
    rig.press(Action::Left);
    rig.press(Action::Accept);
    expect(rig.top().empty() && !rig.controller.active() && rig.session.passwords.empty(),
           "Cancel aborts");

    rig.controller.begin();
    rig.frame();
    rig.press(Action::Back);
    expect(rig.top().empty() && !rig.controller.active(), "B aborts too");
    rig.accept();
    const gpu2d::ui::TextField* field = rig.field();
    expect(rig.controller.typing() && rig.top() == "session.password" && field != nullptr &&
               field->masked() && rig.ui.navigator.typing() && rig.pad.isOpen(),
           "Install asks for the password in a masked field with the keyboard up");
}

void cancellingAtThePasswordWipesIt() {
    Rig rig;
    rig.keyboard.setStyle(input::KeyboardStyle::Regular);
    rig.accept();
    rig.ui.navigator.text("abc");
    rig.button(Button::B);
    expect(rig.typed() == "ab", "B deletes a character");
    rig.button(Button::B);
    rig.button(Button::B);
    rig.button(Button::B);
    expect(!rig.pad.isOpen() && rig.controller.typing() && rig.typed().empty(),
           "B with nothing typed closes the keyboard");
    rig.press(Action::Back);
    expect(!rig.controller.active() && rig.session.passwords.empty(), "B again aborts");
    rig.accept();
    expect(rig.typed().empty(), "the field comes back empty");
    rig.ui.navigator.text("secret");
    rig.press(Action::Back);
    rig.frame();
    expect(!rig.controller.active() && !rig.pad.isOpen() && rig.session.passwords.empty(),
           "Escape aborts");
    rig.accept();
    expect(rig.typed().empty(), "and wiped what was typed");
}

void anEmptyPasswordIsNotSent() {
    Rig rig;
    rig.accept();
    rig.press(Action::Submit);
    expect(rig.session.passwords.empty() && rig.controller.typing() &&
               rig.said == std::vector<std::string>{"error: Type the administrator password first"},
           "nothing is sent, the field stays and the player is told");
}

void aWrongPasswordAsksAgainAndTheRightOneInstalls() {
    Rig rig;
    rig.session.results = {{InstallResult::Kind::Refused, "Sorry, try again."}, {}};
    rig.accept();
    rig.typeAndEnter("wrong");
    expect(rig.controller.installing() && rig.top() == "session.installing" && !rig.pad.isOpen(),
           "the password leaves the field for the install, and a card says it installs");
    rig.press(Action::Back);
    expect(rig.top() == "session.installing", "Back does nothing while it runs");
    rig.finishInstall();
    rig.frame();
    expect(rig.controller.typing() && rig.typed().empty() && rig.pad.isOpen(),
           "a refusal asks again, the field empty and the keyboard up");
    expect(rig.said == std::vector<std::string>{"error: Sorry, try again."}, "and says why");

    rig.typeAndEnter("right");
    rig.finishInstall();
    rig.frame();
    expect(!rig.controller.active() && rig.top().empty(), "the retry installs and ends");
    expect(rig.session.passwords == std::vector<std::string>{"wrong", "right"},
           "each attempt sent what was typed");
    expect(rig.said.back() == "Session mode installed. Power now has Switch to session mode",
           "success is said");
}

void thePadTypesAndDoneSubmits() {
    Rig rig;
    rig.keyboard.setStyle(input::KeyboardStyle::Regular);
    rig.accept();
    rig.focusKey(ui::KeyKind::Character, 'k');
    rig.button(Button::A);
    rig.button(Button::X);
    rig.button(Button::A);
    expect(rig.typed() == "kK", "A types the key and X turns capitals on");
    rig.button(Button::Y);
    expect(rig.typed() == "kK ", "Y types a space");
    rig.button(Button::Start);
    rig.finishInstall();
    expect(rig.session.passwords == std::vector<std::string>{"kK "}, "Start submits");

    Rig done;
    done.keyboard.setStyle(input::KeyboardStyle::Regular);
    done.accept();
    done.ui.navigator.text("pw");
    done.focusKey(ui::KeyKind::Done);
    done.button(Button::A);
    done.finishInstall();
    expect(done.session.passwords == std::vector<std::string>{"pw"}, "the Done key submits");
}

void theWheelTypesThePassword() {
    Rig rig;
    rig.accept();
    expect(rig.keyboard.style() == input::KeyboardStyle::DaisyWheel, "the daisy wheel by default");
    rig.pad.stick(ui::Direction::Up, true);
    rig.button(Button::X);
    rig.button(Button::A);
    rig.button(Button::R2);
    rig.button(Button::B);
    expect(rig.typed() == "adC" && rig.controller.typing(),
           "the face buttons type the petal's characters, B too, and RT steps to capitals");
    rig.pad.stick(ui::Direction::Up, false);
    rig.button(Button::Y);
    rig.button(Button::L1);
    rig.button(Button::X);
    expect(rig.typed() == "ad", "with no petal Y types a space, LB and X delete");
    rig.button(Button::Start);
    rig.finishInstall();
    expect(rig.session.passwords == std::vector<std::string>{"ad"}, "Start submits");

    Rig leave;
    leave.accept();
    leave.ui.navigator.text("secret");
    leave.button(Button::B);
    expect(!leave.pad.isOpen() && leave.controller.typing() && leave.session.passwords.empty(),
           "B with no petal closes the keyboard, the field stays");
}

void anInstallFailureEndsTheFlowWithItsReason() {
    Rig rig;
    rig.session.results = {
        {InstallResult::Kind::Failed, "opensu-install: visudo rejected the sudoers rule"}};
    rig.accept();
    rig.typeAndEnter("pw");
    rig.finishInstall();
    expect(!rig.controller.active() && rig.top().empty(), "the flow ends");
    expect(rig.said ==
               std::vector<std::string>{"error: opensu-install: visudo rejected the sudoers rule"},
           "with the installer's reason");
}

} // namespace

int main() {
    aBlockedInstallSaysWhyAndOpensNothing();
    theDialogExplainsAndCancelAborts();
    cancellingAtThePasswordWipesIt();
    anEmptyPasswordIsNotSent();
    aWrongPasswordAsksAgainAndTheRightOneInstalls();
    thePadTypesAndDoneSubmits();
    theWheelTypesThePassword();
    anInstallFailureEndsTheFlowWithItsReason();
    std::printf("session_install_controller: all checks passed\n");
    return 0;
}
