// Installing session mode: the question on the interface toolkit, Cancel and B, the card while the
// system asks for the password, and what is said when it is installed, cancelled or fails.
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "gpu2d/ui/screen.hpp"

#include "host_fakes.hpp"
#include "session_install_controller.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using host::InstallResult;
using test::expect;

struct Rig {
    Rig()
        : controller{ui.navigator, session, nullptr, sounds,
                     [this](const std::string& text, bool error) {
                         said.push_back((error ? "error: " : "") + text);
                     }} {
    }

    /// Runs the loop until the install has finished.
    void finishInstall() {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (controller.installing() && std::chrono::steady_clock::now() < until) {
            controller.service();
            ui.frame(1.0);
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        expect(!controller.installing(), "the install finished");
    }
    /// The id of the top screen, or empty with none open.
    [[nodiscard]] std::string top() const {
        const gpu2d::ui::Screen* screen = ui.navigator.top();
        return screen == nullptr ? std::string{} : screen->id();
    }
    void press(Action action) {
        ui.navigator.act(action);
        ui.frame(1.0);
    }
    /// Asks and answers Install.
    void accept() {
        controller.begin();
        ui.frame(1.0);
        press(Action::Accept);
    }

    test::InterfaceRig ui;
    audio::SdlSoundPlayer sounds;
    test::FakeSessionMode session;
    std::vector<std::string> said;
    app::SessionInstallController controller;
};

void aBlockedInstallSaysWhyAndOpensNothing() {
    Rig rig;
    rig.session.blocker = "Installing session mode needs polkit's pkexec";
    rig.controller.begin();
    expect(rig.top().empty() &&
               rig.said ==
                   std::vector<std::string>{"error: Installing session mode needs polkit's pkexec"},
           "the reason is said and nothing asks");
}

void theDialogExplainsAndCancelAborts() {
    Rig rig;
    rig.controller.begin();
    rig.ui.frame(1.0);
    const auto* question = dynamic_cast<const gpu2d::ui::Dialog*>(rig.ui.navigator.top());
    expect(question != nullptr && rig.ui.navigator.focused() == "dialog/1",
           "the explanation asks first, on Install");
    expect(question->title() == "Install session mode", "it is titled");
    for (const char* mention :
         {"login session", "Gamescope", "Switch to desktop",
          "/usr/local/libexec/opensu-session-select", "/etc/sudoers.d",
          "/usr/local/share/wayland-sessions", "administrator password", "polkit then asks"}) {
        expect(question->body().find(mention) != std::string::npos, mention);
    }
    rig.press(Action::Left);
    rig.press(Action::Accept);
    expect(rig.top().empty() && !rig.controller.active() && rig.session.installs == 0,
           "Cancel aborts");
    rig.controller.begin();
    rig.ui.frame(1.0);
    rig.press(Action::Back);
    expect(rig.top().empty() && !rig.controller.active() && rig.session.installs == 0,
           "B aborts too");
}

void installWaitsOnACardAndSaysItIsDone() {
    Rig rig;
    rig.accept();
    expect(rig.top() == "session.installing", "a card says the system is asking");
    rig.finishInstall();
    expect(rig.session.installs == 1 && rig.top().empty(), "one install ran and the card closed");
    expect(rig.said == std::vector<std::string>{"Session mode installed. Power now has Switch to "
                                                "session mode"},
           "it says what changed");
}

void aCancelledPasswordIsSaidAsSuch() {
    Rig rig;
    rig.session.results = {{InstallResult::Kind::Cancelled, "Session mode was not installed"}};
    rig.accept();
    rig.finishInstall();
    expect(rig.top().empty() &&
               rig.said == std::vector<std::string>{"Session mode was not installed: "
                                                    "authentication was cancelled"},
           "a dismissed authentication is said, not as an error");
}

void anInstallFailureEndsTheFlowWithItsReason() {
    Rig rig;
    rig.session.results = {
        {InstallResult::Kind::Failed, "opensu-install: visudo rejected the sudoers rule"}};
    rig.accept();
    rig.finishInstall();
    expect(!rig.controller.active() && rig.top().empty(), "the flow ends");
    expect(rig.said ==
               std::vector<std::string>{"error: opensu-install: visudo rejected the sudoers rule"},
           "with the installer's reason");
}

} // namespace

int main() try {
    aBlockedInstallSaysWhyAndOpensNothing();
    theDialogExplainsAndCancelAborts();
    installWaitsOnACardAndSaysItIsDone();
    aCancelledPasswordIsSaidAsSuch();
    anInstallFailureEndsTheFlowWithItsReason();
    std::printf("session_install_controller: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
