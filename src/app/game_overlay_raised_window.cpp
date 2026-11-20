#include "game_overlay.hpp"

namespace opensu::app {
namespace {

/// The shell's own window kept behind the game (Windows): lowered while the game plays, raised in
/// front of it for a menu or page and when the game is over. On a translucent window the game shows
/// through; on an opaque one the menu needs a `GameBackdrop`. macOS has the same for a game it does
/// not embed (`game_overlay_layer_host.mm`). The game keeps the
/// keyboard, so no key is watched in-game; the Guide button reaches the shell through its pads,
/// which SDL reads in the background.
class RaisedWindowOverlay final : public GameOverlay {
  public:
    explicit RaisedWindowOverlay(ShellWindow& window) : window_{window} {
    }

    [[nodiscard]] bool seesGame() const override {
        return window_.translucent();
    }
    void enter() override {
        window_.lower();
    }
    void show(display::OverlayMode mode) override {
        if (mode == display::OverlayMode::Interactive) {
            window_.raise();
        } else {
            window_.lower();
        }
    }
    void leave() override {
        window_.raise();
    }
    std::vector<input::Action> gameKeys(const input::Shortcuts& /*shortcuts*/) override {
        return {};
    }

  private:
    ShellWindow& window_;
};

} // namespace

bool embedsGames() noexcept {
    return false;
}

std::unique_ptr<GameOverlay> openGameOverlay(ShellWindow& window, EmbedSocket& /*embed*/) {
    return std::make_unique<RaisedWindowOverlay>(window);
}

} // namespace opensu::app
