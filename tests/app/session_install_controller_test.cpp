// Installing session mode: the explanation, Cancel, the password on the keyboard (drawn or
// physical), a wrong password and the retry, a failure, and the password gone from the field
// afterwards.
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "host_fakes.hpp"
#include "session_install_controller.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using host::InstallResult;
using test::expect;

struct Rig {
    Rig()
        : controller{dialog, password, session, sounds,
                     [this](const std::string& text, bool error) {
                         said.push_back((error ? "error: " : "") + text);
                     }} {
    }

    /// Runs the loop until the install has finished.
    void finishInstall() {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (dialog.busy() && std::chrono::steady_clock::now() < until) {
            controller.service();
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        expect(!dialog.busy(), "the install finished");
    }
    /// Types `text` on the physical keyboard and presses Enter.
    void typeAndEnter(const std::string& text) {
        controller.typeText(text);
        controller.confirm();
    }
    /// Opens the dialog and accepts it.
    void accept() {
        controller.begin();
        controller.act(Button::A);
    }

    ui::SessionDialog dialog;
    ui::SearchPanel password;
    test::FakeSessionMode session;
    audio::SoundPlayer sounds;
    std::vector<std::string> said;
    app::SessionInstallController controller;
};

void aBlockedInstallSaysWhyAndOpensNothing() {
    Rig rig;
    rig.session.blocker = "openSU is not installed here";
    rig.controller.begin();
    expect(!rig.controller.active() &&
               rig.said == std::vector<std::string>{"error: openSU is not installed here"},
           "the reason is said and no dialog opens");
}

void theDialogExplainsAndCancelAborts() {
    Rig rig;
    rig.controller.begin();
    expect(rig.dialog.isOpen() && rig.controller.active() && !rig.controller.typing(),
           "the explanation opens first");
    const std::string& title = rig.dialog.text().title;
    expect(title == "Install session mode", "it is titled");
    std::string all;
    for (const std::string& paragraph : rig.dialog.text().paragraphs) {
        all += paragraph + "\n";
    }
    for (const char* mention : {"login session", "Gamescope", "Switch to desktop",
                                "/usr/local/libexec/opensu-session-select", "/etc/sudoers.d",
                                "/usr/local/share/wayland-sessions", "administrator password"}) {
        expect(all.find(mention) != std::string::npos, mention);
    }
    expect(rig.dialog.text().accept == "Accept" && rig.dialog.text().cancel == "Cancel",
           "Accept and Cancel");
    rig.controller.act(Button::Right);
    rig.controller.act(Button::A);
    expect(!rig.controller.active() && rig.session.passwords.empty(), "Cancel aborts");

    rig.controller.begin();
    rig.controller.act(Button::B);
    expect(!rig.controller.active(), "B aborts too");
    rig.controller.begin();
    rig.controller.act(Button::Right);
    rig.controller.act(Button::Left);
    rig.controller.act(Button::A);
    expect(rig.password.isOpen() && rig.password.secret() && rig.controller.typing() &&
               !rig.dialog.isOpen(),
           "Accept opens the password keyboard, secret");
}

void cancellingAtThePasswordWipesIt() {
    Rig rig;
    rig.accept();
    rig.controller.typeText("abc");
    rig.controller.act(Button::B);
    expect(rig.password.text() == "ab", "B deletes a character");
    rig.controller.act(Button::Select);
    expect(rig.password.text().empty(), "Select clears");
    rig.controller.act(Button::B);
    expect(!rig.controller.active() && rig.password.text().empty() && rig.session.passwords.empty(),
           "B with nothing typed aborts");
    rig.accept();
    rig.controller.typeText("secret");
    rig.controller.dismiss();
    expect(!rig.controller.active() && rig.password.text().empty(),
           "Escape aborts and wipes the field");
}

void anEmptyPasswordIsNotSent() {
    Rig rig;
    rig.accept();
    rig.controller.confirm();
    expect(rig.session.passwords.empty() && rig.password.isOpen() &&
               rig.said == std::vector<std::string>{"error: Type the administrator password first"},
           "nothing is sent, the keyboard stays and the player is told");
}

void aWrongPasswordAsksAgainAndTheRightOneInstalls() {
    Rig rig;
    rig.session.results = {{InstallResult::Kind::Refused, "Sorry, try again."}, {}};
    rig.accept();
    rig.typeAndEnter("wrong");
    expect(rig.dialog.busy() && !rig.password.isOpen() && rig.password.text().empty(),
           "the password leaves the field for the install");
    rig.controller.act(Button::B);
    expect(rig.dialog.busy(), "buttons do nothing while it runs");
    rig.finishInstall();
    expect(rig.password.isOpen() && rig.controller.typing() && rig.password.text().empty() &&
               !rig.dialog.isOpen(),
           "a refusal brings the keyboard back, empty");
    expect(rig.said == std::vector<std::string>{"error: Sorry, try again."}, "and says why");

    rig.typeAndEnter("right");
    rig.finishInstall();
    expect(!rig.controller.active() && rig.password.text().empty(), "the retry installs and ends");
    expect(rig.session.passwords == std::vector<std::string>{"wrong", "right"},
           "each attempt sent what was typed");
    expect(rig.said.back() == "Session mode installed. Power now has Switch to session mode",
           "success is said");
}

void thePadTypesAndDoneSubmits() {
    Rig rig;
    rig.accept();
    const auto keys = ui::searchKeys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].kind == ui::KeyKind::Character && keys[i].character == 'k') {
            rig.password.focusKey(i);
            break;
        }
    }
    rig.controller.act(Button::A);
    rig.controller.act(Button::X);
    rig.controller.act(Button::A);
    expect(rig.password.text() == "kK", "A types the key and X turns capitals on");
    rig.controller.act(Button::Y);
    expect(rig.password.text() == "kK ", "Y types a space");
    rig.controller.act(Button::Start);
    rig.finishInstall();
    expect(rig.session.passwords == std::vector<std::string>{"kK "}, "Start submits");

    Rig done;
    done.accept();
    done.controller.typeText("pw");
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].kind == ui::KeyKind::Done) {
            done.password.focusKey(i);
        }
    }
    done.controller.act(Button::A);
    done.finishInstall();
    expect(done.session.passwords == std::vector<std::string>{"pw"}, "the Done key submits");
}

void anInstallFailureEndsTheFlowWithItsReason() {
    Rig rig;
    rig.session.results = {
        {InstallResult::Kind::Failed, "opensu-install: visudo rejected the sudoers rule"}};
    rig.accept();
    rig.typeAndEnter("pw");
    rig.finishInstall();
    expect(!rig.controller.active() && rig.password.text().empty(), "the flow ends");
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
    anInstallFailureEndsTheFlowWithItsReason();
    std::printf("session_install_controller: all checks passed\n");
    return 0;
}
