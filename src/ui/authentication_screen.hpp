// authentication_screen — a card over the dimmed screen for an authentication under way: whose it
// is, what the system says now and what it is for, a sweeping bar while it waits on something
// other than the player (a finger, a face, a phone), and a field only while an answer is asked
// for, masked for a secret. The card's content is read on every build, so it changes in place;
// Submit (Enter, or the keyboard's Done) hands the field over and B cancels.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::ui {

/// What the card shows now.
struct AuthenticationCard {
    std::string title;
    /// Under the title; empty for none.
    std::string note;
    /// The field asked for, by its label; nothing while the card waits.
    struct Field {
        std::string label;
        bool secret{true};
    };
    std::optional<Field> field;
};

/// What the card reads and what it does with the answer.
struct Authenticating {
    std::string id;
    std::function<AuthenticationCard()> card;
    std::size_t mostBytes{};
    /// The field's text as it stands.
    std::function<const std::string&()> text;
    std::function<void(std::string)> changed;
    std::function<void()> submitted;
    std::function<void()> cancelled;
};

class AuthenticationScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* fieldId = "authentication/field";

    explicit AuthenticationScreen(Authenticating authenticating)
        : Screen{authenticating.id, {}}, authenticating_{std::move(authenticating)} {
    }

    [[nodiscard]] std::string title() const override {
        return authenticating_.card().title;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override {
        return fieldId;
    }

  private:
    Authenticating authenticating_;
};

} // namespace opensu::ui
