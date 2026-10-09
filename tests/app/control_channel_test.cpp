// The control channel's routing, with a fake shell and a fake sign-in.
#include "control_channel.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using iideck::app::ControlChannel;
using iideck::app::ControlTarget;
using iideck::app::ShellSnapshot;
using iideck::app::SignInResult;
using iideck::app::SignInService;
using iideck::app::Store;
using lucent::http::Request;
using lucent::http::Response;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

class FakeShell final : public ControlTarget {
  public:
    [[nodiscard]] ShellSnapshot snapshot() const override {
        return state;
    }
    void inject(iideck::gamepad::Button button, iideck::input::Device device) override {
        pressed.push_back(button);
        devices.push_back(device);
    }
    [[nodiscard]] bool captureFrame(std::string& /*png*/) override {
        return false;
    }
    void requestClose() override {
    }
    void requestCatalogReload(std::string toast) override {
        reloads.push_back(std::move(toast));
    }

    std::vector<std::string> reloads;
    std::vector<iideck::gamepad::Button> pressed;
    std::vector<iideck::input::Device> devices;
    ShellSnapshot state;
};

class FakeSignIn final : public SignInService {
  public:
    [[nodiscard]] SignInResult open(Store store) override {
        opened.push_back(store);
        return {openOk, "opened"};
    }
    [[nodiscard]] SignInResult complete(Store store, std::string_view code) override {
        completed.emplace_back(store, std::string{code});
        return {completeOk, completeOk ? "signed in" : "refused"};
    }

    bool openOk{true};
    bool completeOk{true};
    std::vector<Store> opened;
    std::vector<std::pair<Store, std::string>> completed;
};

Request post(std::string target, std::string body = {}) {
    return Request{
        .method = "POST", .target = std::move(target), .headers = {}, .body = std::move(body)};
}

void testStart() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};

    Response gog = channel.handle(post("/signin/gog/start"));
    expect(gog.status == 200 && signIn.opened == std::vector<Store>{Store::Gog},
           "GOG's start opens its page");
    expect(contains(gog.body, "\"ok\":true") && contains(gog.body, "\"store\":\"GOG\""),
           "start answers with the store and the outcome");
    Response epic = channel.handle(post("/signin/epic/start"));
    expect(epic.status == 200 && signIn.opened.back() == Store::Epic,
           "Epic's start opens its page");
    expect(shell.reloads.empty(), "opening a page reloads nothing");

    signIn.openOk = false;
    expect(channel.handle(post("/signin/gog/start")).status == 502,
           "a page that cannot open is reported");
}

void testComplete() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};

    Response gog = channel.handle(post("/signin/gog", "  abc-123_X.y~\r\n"));
    expect(gog.status == 200 && signIn.completed.size() == 1 &&
               signIn.completed[0] == std::pair{Store::Gog, std::string{"abc-123_X.y~"}},
           "the code in the body, trimmed, finishes GOG's sign-in");
    expect(shell.reloads == std::vector<std::string>{"signed in"},
           "a finished sign-in reloads the catalog");
    expect(!contains(gog.body, "abc-123"), "the answer never repeats the code");

    Response epic = channel.handle(post("/signin/epic", "0123456789abcdef"));
    expect(epic.status == 200 && signIn.completed.back().first == Store::Epic,
           "the code finishes Epic's sign-in");

    signIn.completeOk = false;
    shell.reloads.clear();
    expect(channel.handle(post("/signin/gog", "rejected")).status == 502 && shell.reloads.empty(),
           "a refused code is reported and reloads nothing");
}

void testRefusals() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};

    for (const char* body : {"", "   ", "-x", "two words", "a;b", "a/b", "{\"code\":\"x\"}"}) {
        expect(channel.handle(post("/signin/gog", body)).status == 400,
               "a body that is not a code is refused");
    }
    expect(channel.handle(post("/signin/gog", std::string(600, 'a'))).status == 400,
           "an over-long code is refused");
    expect(signIn.completed.empty(), "a refused body reaches no store");
    expect(channel
                   .handle(Request{
                       .method = "GET", .target = "/signin/gog/start", .headers = {}, .body = {}})
                   .status == 400,
           "sign-in is POST only");
    expect(channel.handle(post("/signin/steam")).status == 404, "only GOG and Epic have sign-in");
    expect(channel.handle(post("/signin/steam/start")).status == 404,
           "only GOG and Epic have a sign-in page");
}

void testState() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    const Request get{.method = "GET", .target = "/state", .headers = {}, .body = {}};

    Response fresh = channel.handle(get);
    expect(fresh.status == 200 && contains(fresh.body, "\"section\":\"home\"") &&
               contains(fresh.body, "\"libraryMode\":\"standard\"") &&
               contains(fresh.body, "\"modeChooserOpen\":false"),
           "the state names the section, Library's mode and whether the picker is up");

    shell.state.section = "library";
    shell.state.libraryMode = "carousel";
    shell.state.modeChooserOpen = true;
    shell.state.shelf = "ps2";
    Response library = channel.handle(get);
    expect(contains(library.body, "\"section\":\"library\"") &&
               contains(library.body, "\"libraryMode\":\"carousel\"") &&
               contains(library.body, "\"modeChooserOpen\":true") &&
               contains(library.body, "\"shelf\":\"ps2\""),
           "the state follows the shell");
}

void testSectionButtons() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    using iideck::gamepad::Button;

    for (const char* name : {"l1", "r1", "start"}) {
        expect(channel.handle(post("/input", name)).status == 200, "the dock's buttons are input");
    }
    expect(shell.pressed == std::vector<Button>{Button::L1, Button::R1, Button::Start},
           "L1, R1 and START reach the shell as those buttons");
}

void testKeyboardInput() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    using iideck::input::Device;

    expect(channel.handle(post("/input", "a")).status == 200, "a pad press is input");
    expect(channel.handle(post("/input", "a keyboard")).status == 200, "a key press is input");
    expect(shell.devices == std::vector<Device>{Device::Pad, Device::KeyboardMouse},
           "the word keyboard says the press came from a key");
    expect(channel.handle(post("/input", "a mouse")).status == 400, "no other device is named");
    expect(shell.devices.size() == 2, "a refused press reaches nobody");
}

} // namespace

int main() {
    testState();
    testKeyboardInput();
    testSectionButtons();
    testStart();
    testComplete();
    testRefusals();
    std::printf("control channel: all checks passed\n");
    return 0;
}
