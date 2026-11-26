// fumarole_qr_screen — "Sign in to Steam" while fumarole waits for the Steam mobile app: what to do
// on the phone beside the QR code fumarole writes, shown again each time Steam refreshes it. Back
// ends the sign-in.
#pragma once

#include <memory>

#include "gpu2d/ui/screen.hpp"

#include "fumarole_sign_in.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class FumaroleQrScreen final : public gpu2d::ui::Screen {
  public:
    FumaroleQrScreen(FumaroleSignIn& signIn, ui::ScreenImages& images, const ui::Chrome& chrome)
        : Screen{"wizard/fumarole-qr", "Sign in to Steam"}, signIn_{signIn}, images_{images},
          chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;

  private:
    FumaroleSignIn& signIn_;
    ui::ScreenImages& images_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app
