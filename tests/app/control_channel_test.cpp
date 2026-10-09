// The control channel's routing, with a fake shell and a fake sign-in.
#include "control_channel.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ui/check.hpp"

namespace {

using lucent::http::Request;
using lucent::http::Response;
using opensu::app::ControlChannel;
using opensu::app::ControlTarget;
using opensu::app::ShellSnapshot;
using opensu::app::SignInResult;
using opensu::app::SignInService;
using opensu::app::Store;

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
    void typeText(std::string text) override {
        typed.push_back(std::move(text));
    }
    void inject(opensu::gamepad::Button button, opensu::input::Device device) override {
        pressed.push_back(button);
        devices.push_back(device);
    }
    void injectKey(opensu::input::Combo combo) override {
        keys.push_back(combo);
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
    std::vector<std::string> typed;
    std::vector<opensu::input::Combo> keys;
    std::vector<opensu::gamepad::Button> pressed;
    std::vector<opensu::input::Device> devices;
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

void testSearchAndText() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    using opensu::gamepad::Button;

    expect(channel.handle(post("/input", "search")).status == 200 &&
               shell.pressed == std::vector<Button>{Button::Search},
           "the search button is input");
    expect(channel.handle(post("/text", "had")).status == 200 &&
               shell.typed == std::vector<std::string>{"had"},
           "text reaches the shell");
    const Request get{.method = "GET", .target = "/text", .headers = {}, .body = {}};
    expect(channel.handle(get).status != 200 && shell.typed.size() == 1, "text is POST only");

    shell.state.searchOpen = true;
    shell.state.searchText = "had";
    shell.state.contextMenuOpen = true;
    shell.state.iconSize = 14;
    shell.state.detailsOpen = true;
    shell.state.settingsOpen = true;
    shell.state.folderPickerOpen = true;
    shell.state.breadcrumb = "Library > GOG > Hades";
    const Response state =
        channel.handle(Request{.method = "GET", .target = "/state", .headers = {}, .body = {}});
    expect(contains(state.body, "\"searchOpen\":true") &&
               contains(state.body, "\"searchText\":\"had\"") &&
               contains(state.body, "\"contextMenuOpen\":true") &&
               contains(state.body, "\"iconSize\":14") &&
               contains(state.body, "\"detailsOpen\":true") &&
               contains(state.body, "\"settingsOpen\":true") &&
               contains(state.body, "\"folderPickerOpen\":true") &&
               contains(state.body, "\"breadcrumb\":\"Library > GOG > Hades\""),
           "the state reports the search, the menu, the icon size, the details page and the trail");
}

void testSectionButtons() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    using opensu::gamepad::Button;

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
    using opensu::input::Device;

    expect(channel.handle(post("/input", "a")).status == 200, "a pad press is input");
    expect(channel.handle(post("/input", "a keyboard")).status == 200, "a key press is input");
    expect(shell.devices == std::vector<Device>{Device::Pad, Device::KeyboardMouse},
           "the word keyboard says the press came from a key");
    expect(channel.handle(post("/input", "a mouse")).status == 400, "no other device is named");
    expect(shell.devices.size() == 2, "a refused press reaches nobody");
}

void testKeysAndVolumeState() {
    FakeShell shell;
    FakeSignIn signIn;
    ControlChannel channel{shell, signIn, 0};
    using opensu::input::Combo;

    expect(channel.handle(post("/key", "ctrl+Up")).status == 200 &&
               channel.handle(post("/key", "f9")).status == 200,
           "a spelled key is input");
    expect(shell.keys ==
               (std::vector<Combo>{
                   {opensu::test::need(opensu::input::comboNamed("ctrl+up"), "a key name").key,
                    true, false},
                   {opensu::test::need(opensu::input::comboNamed("f9"), "a key name").key, false,
                    false}}),
           "it reaches the shell as that combination");
    expect(channel.handle(post("/key", "ctrl+nothing")).status == 400 &&
               channel.handle(post("/key", "")).status == 400,
           "an unknown key is refused");
    expect(shell.keys.size() == 2, "a refused key reaches nobody");
    expect(channel.handle(post("/input", "l2")).status == 200, "L2 can be injected too");

    shell.state.volumePercent = 45;
    shell.state.volumeMuted = true;
    shell.state.volumeShown = true;
    shell.state.uiScale = 125;
    shell.state.capturingShortcut = true;
    const Response state =
        channel.handle(Request{.method = "GET", .target = "/state", .headers = {}, .body = {}});
    expect(contains(state.body, "\"volumePercent\":45") &&
               contains(state.body, "\"volumeMuted\":true") &&
               contains(state.body, "\"volumeShown\":true") &&
               contains(state.body, "\"uiScale\":125") &&
               contains(state.body, "\"capturingShortcut\":true"),
           "the state reports the volume, the scale and the shortcut capture");
}

} // namespace

int main() {
    testKeysAndVolumeState();
    testState();
    testKeyboardInput();
    testSearchAndText();
    testSectionButtons();
    testStart();
    testComplete();
    testRefusals();
    std::printf("control channel: all checks passed\n");
    return 0;
}
