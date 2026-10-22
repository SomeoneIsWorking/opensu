#include "game_overlay.hpp"
#include "session/game_keys.hpp"
#include "session/gamescope_overlay.hpp"

namespace opensu::app {
namespace {

class GamescopeGameOverlay final : public GameOverlay {
  public:
    explicit GamescopeGameOverlay(std::uint64_t window) : overlay_{window} {
    }

    void enter() override {
        overlay_.enter();
    }
    void show(session::OverlayMode mode) override {
        overlay_.show(mode);
    }
    void leave() override {
        overlay_.leave();
    }
    std::vector<input::Action> gameKeys(const input::Shortcuts& shortcuts) override {
        return keys_.poll(shortcuts);
    }

  private:
    session::GamescopeOverlay overlay_;
    session::GameKeys keys_;
};

} // namespace

std::unique_ptr<GameOverlay> openGameOverlay(const ShellWindow& window) {
    return std::make_unique<GamescopeGameOverlay>(window.x11Window());
}

} // namespace opensu::app
