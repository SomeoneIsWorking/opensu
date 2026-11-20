#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "embed_socket.hpp"
#include "game_typing.hpp"

namespace opensu::app {
namespace {

/// The Mac's virtual key codes on the US layout (Carbon's kVK_*), which winemac turns into the
/// keys a Windows game reads.
namespace mac {
constexpr int Return = 36;
constexpr int Delete = 51;
constexpr int Shift = 56;
constexpr int Left = 123;
constexpr int Right = 124;
/// NSEventModifierFlagShift.
constexpr std::uint64_t ShiftFlag = std::uint64_t{1} << 17U;
} // namespace mac

struct Stroke {
    int keycode{-1};
    bool shift{false};
};

/// What each key code from kVK_ANSI_A (0) to kVK_ANSI_Grave (50) types on the US layout, alone
/// and with Shift; a NUL where the key types nothing (the ISO section key, 10).
constexpr std::string_view plain{"asdfhgzxcv\0bqweryt123465=97-80]ou[ip\rlj'k;\\,/nm.\t `", 51};
constexpr std::string_view shifted{"ASDFHGZXCV\0BQWERYT!@#$^%+(&_*)}OU{IP\rLJ\"K:|<?NM>\t ~", 51};

/// The key that types `character` on the US layout, or keycode -1 for none.
Stroke strokeOf(char character) {
    if (character == '\0') {
        return {};
    }
    if (character == '\n') {
        character = '\r';
    }
    if (const std::size_t code = plain.find(character); code != std::string_view::npos) {
        return {static_cast<int>(code), false};
    }
    if (const std::size_t code = shifted.find(character); code != std::string_view::npos) {
        return {static_cast<int>(code), true};
    }
    return {};
}

/// Types into the Wine game embedded in openSU's window: each character a key down and up, as
/// the Mac keyboard would send it, to the process whose window is on top.
class EmbeddedGameTyping final : public GameTyping {
  public:
    explicit EmbeddedGameTyping(EmbedSocket& embed) : embed_{embed} {
    }

    bool type(std::string_view text) override {
        bool all = true;
        for (const char character : text) {
            const Stroke stroke = strokeOf(character);
            if (stroke.keycode < 0) {
                all = false;
                continue;
            }
            press(stroke.keycode, std::string(1, character), stroke.shift);
        }
        return all;
    }
    void erase() override {
        press(mac::Delete, "\x7f", false);
    }
    void enter() override {
        press(mac::Return, "\r", false);
    }
    void caret(bool left) override {
        // NSEvent's function-key characters for the arrows (NSLeftArrowFunctionKey and the
        // right one), as UTF-8.
        press(left ? mac::Left : mac::Right, left ? "\xef\x9c\x82" : "\xef\x9c\x83", false);
    }

  private:
    void press(int keycode, const std::string& chars, bool shift) {
        const std::uint64_t modifiers = shift ? mac::ShiftFlag : 0;
        if (shift) {
            embed_.key({.down = true, .keycode = mac::Shift, .chars = {}, .modifiers = modifiers});
        }
        embed_.key({.down = true, .keycode = keycode, .chars = chars, .modifiers = modifiers});
        embed_.key({.down = false, .keycode = keycode, .chars = chars, .modifiers = modifiers});
        if (shift) {
            embed_.key({.down = false, .keycode = mac::Shift, .chars = {}, .modifiers = 0});
        }
    }

    EmbedSocket& embed_;
};

} // namespace

bool typesIntoGames(bool /*insideGamescope*/, const EmbedSocket& embed) noexcept {
    return embed.isOpen() && !embed.windows().empty();
}

std::unique_ptr<GameTyping> openGameTyping(const std::string& /*x11Display*/, EmbedSocket& embed) {
    return std::make_unique<EmbeddedGameTyping>(embed);
}

} // namespace opensu::app
