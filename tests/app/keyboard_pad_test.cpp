// The pad's keyboard over a game: what the wheel and the grid type goes to the game, which keeps
// the text, and is echoed on the preview line; Delete, the caret and Start are the game's
// Backspace, arrows and Return; it closes with Start, with B on an empty line, when a screen opens
// over it and when the game ends.
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"

#include "keyboard_pad.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gamepad::Button;
using test::expect;

class FakeTyping final : public app::GameTyping {
  public:
    bool type(std::string_view text) override {
        sent.push_back(std::string{text});
        return true;
    }
    void erase() override {
        sent.emplace_back("<erase>");
    }
    void enter() override {
        sent.emplace_back("<enter>");
    }
    void caret(bool left) override {
        sent.emplace_back(left ? "<left>" : "<right>");
    }

    std::vector<std::string> sent;
};

class CoverPage final : public gpu2d::ui::Screen {
  public:
    CoverPage() : Screen{"cover", "Cover"} {
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget>
    build(const gpu2d::ui::Theme& /*theme*/) override {
        return std::make_unique<gpu2d::ui::Column>();
    }
};

struct Rig {
    test::InterfaceRig ui;
    ui::OnScreenKeyboard keyboard;
    audio::SdlSoundPlayer sounds;
    app::KeyboardPad pad{keyboard, ui.navigator, sounds};
    FakeTyping game;
};

std::string previewOf(const Rig& rig) {
    return rig.keyboard.preview().value_or("<none>");
}

void theWheelTypesIntoTheGame() {
    Rig rig;
    rig.pad.openOverGame(rig.game);
    expect(rig.pad.isOpen() && rig.pad.overGame() && previewOf(rig).empty(),
           "it opens over the game with an empty preview line");
    rig.pad.stick(ui::Direction::Up, true);
    rig.pad.act(Button::A);
    const std::string typed(1, ui::daisyCharacter(ui::DaisySet::Letters, 0, ui::DaisyFace::A));
    expect(rig.game.sent == std::vector<std::string>{typed} && previewOf(rig) == typed,
           "a petal's character goes to the game and shows on the preview line");
    rig.pad.stick(ui::Direction::Up, false);
    rig.pad.act(Button::Y);
    rig.pad.act(Button::L1);
    expect(rig.game.sent.size() == 3 && rig.game.sent[1] == " " && rig.game.sent[2] == "<erase>" &&
               previewOf(rig) == typed,
           "Y is a space and LB the game's Backspace, the preview following");
    rig.pad.act(Button::Left);
    rig.pad.act(Button::Select);
    rig.pad.act(Button::A);
    expect(rig.game.sent.back() == "<left>" && rig.game.sent.size() == 4,
           "the d-pad moves the game's caret; Clear and Paste have nothing to act on");
    rig.pad.follow(true);
    expect(rig.pad.isOpen(), "a frame over the running game keeps it up");
    rig.pad.act(Button::Start);
    expect(rig.game.sent.back() == "<enter>" && !rig.pad.isOpen() && !rig.pad.overGame() &&
               !rig.keyboard.preview(),
           "Start is the game's Return and closes it");
}

void theGridClosesOnAnEmptyLine() {
    Rig rig;
    rig.keyboard.setStyle(input::KeyboardStyle::Regular);
    rig.pad.openOverGame(rig.game);
    rig.pad.act(Button::A);
    expect(rig.game.sent == std::vector<std::string>{"1"} && previewOf(rig) == "1",
           "the grid's first key types into the game");
    rig.pad.act(Button::B);
    expect(rig.game.sent.back() == "<erase>" && previewOf(rig).empty() && rig.pad.isOpen(),
           "B deletes what it typed");
    rig.pad.act(Button::B);
    expect(!rig.pad.isOpen() && rig.game.sent.size() == 2, "B on an empty line closes it");
}

void itClosesWhenAScreenOpensOrTheGameEnds() {
    Rig rig;
    rig.pad.openOverGame(rig.game);
    rig.ui.navigator.push(std::make_unique<CoverPage>());
    rig.pad.follow(true);
    expect(!rig.pad.isOpen(), "a screen over it, the Guide menu, takes the buttons back");
    rig.ui.navigator.clear();
    rig.ui.frame(1.0);
    rig.pad.openOverGame(rig.game);
    rig.pad.follow(false);
    expect(!rig.pad.isOpen() && !rig.pad.overGame(), "a game that ends takes it down");
}

} // namespace

int main() try {
    theWheelTypesIntoTheGame();
    theGridClosesOnAnEmptyLine();
    itClosesWhenAScreenOpensOrTheGameEnds();
    std::printf("keyboard_pad: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
