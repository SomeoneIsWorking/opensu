// integration_wizard — adding an integration from Library's Add page. Steam asks how its games
// come: fumarole, then its sign-in (a QR code for the Steam mobile app, this PC's Steam login, or
// later); or the Steam client, then its mode where there is a choice (a login session always runs
// openSU's own), and the client asks for its login when it starts. Epic and GOG are added and their
// sign-in opens; the others are just added. Each step is a page over the last; Back steps back, and
// only the last step adds anything.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "gpu2d/ui/navigator.hpp"

#include "fumarole_sign_in.hpp"
#include "integrations_in_force.hpp"
#include "sign_in.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class IntegrationWizard final : public ui::RowActions {
  public:
    struct Hooks {
        /// Keeps `choices` and puts them in effect.
        std::function<void(const settings::IntegrationChoices& choices)> add;
        /// Opens `store`'s sign-in.
        std::function<void(Store store)> signIn;
        std::function<void(const std::string& text, bool error)> toast;
    };

    /// The pages of Steam's steps.
    enum class Step : std::uint8_t { Kind, Mode, Login };

    IntegrationWizard(const IntegrationsInForce& integrations, gpu2d::ui::Navigator& navigator,
                      const ui::Chrome& chrome, ui::ScreenImages& images, FumaroleSignIn& fumarole,
                      Hooks hooks)
        : integrations_{integrations}, navigator_{navigator}, chrome_{chrome}, images_{images},
          fumarole_{fumarole}, hooks_{std::move(hooks)} {
    }

    /// Adds the integration entry `entry` (`integrationEntries`), Steam through its steps.
    void start(std::string_view entry);
    /// Step `step`'s title and rows.
    [[nodiscard]] ui::SettingsCategory page(Step step) const;
    /// A pick on a step's page.
    void run(const std::string& id, int step) override;
    /// Takes how a fumarole sign-in ended: adds Steam when it signed in. Each frame.
    void service();

  private:
    void show(Step step);
    /// Adds what is staged, closes the steps and says `text`.
    void finish(const std::string& text);
    /// Closes the first open page whose id starts with `id` and every page over it.
    void closeFrom(std::string_view id);

    const IntegrationsInForce& integrations_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
    FumaroleSignIn& fumarole_;
    Hooks hooks_;
    /// The choices the steps build on.
    settings::IntegrationChoices staged_;
    /// A fumarole sign-in this started is under way.
    bool signingIn_{false};
};

} // namespace opensu::app
