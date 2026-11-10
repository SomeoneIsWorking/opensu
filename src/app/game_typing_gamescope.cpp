#include "game_typing.hpp"
#include "session/xtest_input.hpp"

namespace opensu::app {
namespace {

class XTestGameTyping final : public GameTyping {
  public:
    explicit XTestGameTyping(const std::string& x11Display) : input_{x11Display} {
    }

    bool type(std::string_view text) override {
        bool all = true;
        for (const char character : text) {
            all = input_.type(character) && all;
        }
        return all;
    }
    void erase() override {
        input_.tap(session::EditKey::Backspace);
    }
    void enter() override {
        input_.tap(session::EditKey::Return);
    }
    void caret(bool left) override {
        input_.tap(left ? session::EditKey::Left : session::EditKey::Right);
    }

  private:
    session::XTestInput input_;
};

} // namespace

bool typesIntoGames(bool insideGamescope) noexcept {
    return insideGamescope;
}

std::unique_ptr<GameTyping> openGameTyping(const std::string& x11Display) {
    return std::make_unique<XTestGameTyping>(x11Display);
}

} // namespace opensu::app
