// import_picks_screen — the games found to import, as a card over the dimmed screen: a switch per
// game (all on at first), Select all, Deselect all, Undo of the last change and Import selected.
// Back closes it importing nothing.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "import_picks.hpp"

namespace opensu::app {

inline constexpr std::string_view importPicksId = "import";
inline constexpr std::string_view importAllId = "import/all";
inline constexpr std::string_view importNoneId = "import/none";
inline constexpr std::string_view importUndoId = "import/undo";
inline constexpr std::string_view importDoneId = "import/done";

/// The switch of game `index`.
[[nodiscard]] std::string importGameId(std::size_t index);

class ImportPicksScreen final : public gpu2d::ui::Screen {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The games chosen, once Import selected closes the card.
        std::function<void(std::vector<library::own::Entry> picked)> imported;
    };

    /// `title` names where the games come from; `home` shortens their paths.
    ImportPicksScreen(std::string title, ImportPicks picks, std::filesystem::path home, Hooks hooks)
        : Screen{std::string{importPicksId}, std::move(title)}, picks_{std::move(picks)},
          home_{std::move(home)}, hooks_{std::move(hooks)} {
    }

    [[nodiscard]] const ImportPicks& picks() const noexcept {
        return picks_;
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    void undo();
    void import();

    ImportPicks picks_;
    std::filesystem::path home_;
    Hooks hooks_;
};

} // namespace opensu::app
