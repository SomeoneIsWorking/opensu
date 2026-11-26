// library_add_screen — the "Add to library" page on the interface toolkit: its title beside the rows
// `LibraryAdditions` lists, or above them on a narrow screen.
#pragma once

#include <memory>
#include <string>

#include "gpu2d/ui/screen.hpp"

#include "ui/screen_stack.hpp"

namespace opensu::app {

class LibraryAdditions;

class LibraryAddScreen final : public gpu2d::ui::Screen {
  public:
    LibraryAddScreen(LibraryAdditions& additions, const ui::Chrome& chrome)
        : Screen{"library-add", {}}, additions_{additions}, chrome_{chrome} {
    }

    [[nodiscard]] std::string title() const override {
        return title_;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    LibraryAdditions& additions_;
    const ui::Chrome& chrome_;
    /// The page's title as last built.
    std::string title_;
};

} // namespace opensu::app
