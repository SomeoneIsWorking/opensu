// touch_layer — on-screen controls for a touchscreen with no controller, the runner's and the ones
// over a remote game. It runs a `ControlPlan` (`pad_layout`): each finger keeps the control it went
// down on, so several are held at once and a thumb slides within one; the buttons held reach the
// game as a pad's do, and the pause button holds Guide, which asks for the shell's menu as a pad's
// Guide does. The pieces are composed into an overlay the size of the surface.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include "gamepad/event.hpp"
#include "touch/touch_art.hpp"
#include "touch/touch_controls.hpp"
#include "touch/touch_surface.hpp"

namespace opensu::touch {

class TouchLayer {
  public:
    /// The art is read from `art` (assets/touch).
    explicit TouchLayer(std::filesystem::path art);

    /// Places the controls on `surface`; fingers down are let go.
    void layout(const WindowSurface& surface);

    /// Takes a finger; reports whether the buttons held changed.
    bool finger(const Finger& finger);
    /// Lets every finger go; reports whether any button was held.
    bool releaseAll();
    /// The buttons the fingers hold now.
    [[nodiscard]] std::vector<gamepad::Button> held() const;
    /// Whether a finger holds `button`.
    [[nodiscard]] bool pressed(gamepad::Button button) const;

    /// The controls as an overlay the size of the surface, drawn again when what is held changed.
    [[nodiscard]] OverlayImage image();

  private:
    struct Touch {
        std::size_t control{0};
        std::vector<gamepad::Button> buttons;
    };

    /// The index of the control a finger at `x`, `y` takes.
    [[nodiscard]] std::optional<std::size_t> controlAt(float x, float y) const;
    [[nodiscard]] std::vector<gamepad::Button> buttonsAt(const Control& control, float x,
                                                         float y) const;
    void compose(const Art& art, const Box& box, float opacity);
    void draw();

    ArtSource art_;
    ControlPlan plan_;
    unsigned width_{0};
    unsigned height_{0};
    std::map<std::uint64_t, Touch> touches_;
    std::vector<unsigned char> pixels_;
    bool dirty_{true};
};

} // namespace opensu::touch
