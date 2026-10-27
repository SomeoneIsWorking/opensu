// entry_screen — a line typed in a card over the dimmed screen, for what is asked once and kept
// out of view: a title, a note, and the field, masked for a secret. The field's text is the
// caller's; Submit (Enter, or the keyboard's Done) hands it over and B cancels.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::ui {

/// What an entry asks and what it does with the answer.
struct Entry {
    std::string id;
    std::string title;
    /// Under the title; empty for none.
    std::string note;
    /// The field's label and what it says while empty.
    std::string label;
    std::string placeholder;
    bool secret{false};
    std::size_t mostBytes{};
    /// The text as it stands.
    std::function<const std::string&()> text;
    std::function<void(std::string)> changed;
    std::function<void()> submitted;
    std::function<void()> cancelled;
};

class EntryScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* fieldId = "entry/field";

    explicit EntryScreen(Entry entry) : Screen{entry.id, entry.title}, entry_{std::move(entry)} {
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
    Entry entry_;
};

} // namespace opensu::ui
