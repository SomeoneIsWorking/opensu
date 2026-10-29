// touch_layer — the runner's on-screen pad for a touchscreen with no controller: a d-pad, A B X Y,
// L and R, Select and Start, and a pause button that holds Guide, which asks for the shell's menu
// as a pad's Guide does. Its buttons reach the game as a pad's do. Controls sit inside the window's
// safe area, sized in dp, their hit regions larger than what is drawn; each finger keeps the
// control it went down on, so several are held at once and a thumb slides within the d-pad or the
// face buttons. The art is assets/touch/, rasterised at the drawn size and composed into the
// overlay.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "gamepad/event.hpp"
#include "overlay_pass.hpp"
#include "window_input.hpp"

namespace opensu::retro {

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
    enum class Control : std::uint8_t {
        Pad,
        Face,
        L1,
        R1,
        Select,
        Start,
        Pause,
    };
    struct Box {
        float x{0.0F};
        float y{0.0F};
        float width{0.0F};
        float height{0.0F};

        [[nodiscard]] bool contains(float px, float py) const {
            return px >= x && py >= y && px < x + width && py < y + height;
        }
        [[nodiscard]] Box grown(float by) const {
            return {x - by, y - by, width + (2.0F * by), height + (2.0F * by)};
        }
    };
    struct Art {
        std::vector<unsigned char> rgba;
        int width{0};
        int height{0};
    };
    struct Touch {
        Control control{Control::Pad};
        std::vector<gamepad::Button> buttons;
    };

    [[nodiscard]] std::optional<Control> controlAt(float x, float y) const;
    [[nodiscard]] std::vector<gamepad::Button> buttonsAt(Control control, float x, float y) const;
    /// `file`'s art rasterised to fill `box`.
    [[nodiscard]] Art rasterised(const char* file, const Box& box) const;
    void compose(const Art& art, const Box& box, float opacity);
    void draw();

    std::filesystem::path artDir_;
    float dp_{1.0F};
    unsigned width_{0};
    unsigned height_{0};
    Box pad_;
    /// A, B, X, Y.
    std::array<Box, 4> face_{};
    Box faceCluster_;
    Box l1_;
    Box r1_;
    Box select_;
    Box start_;
    Box pause_;
    Art padArt_;
    std::array<Art, 4> faceArt_{};
    Art l1Art_;
    Art r1Art_;
    Art selectArt_;
    Art startArt_;
    Art pauseArt_;
    std::map<std::uint64_t, Touch> touches_;
    std::vector<unsigned char> pixels_;
    bool dirty_{true};
};

} // namespace opensu::retro
