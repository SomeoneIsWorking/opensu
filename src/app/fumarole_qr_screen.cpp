#include "fumarole_qr_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/image_view.hpp"
#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* steps =
    "Open the Steam app on your phone, tap the shield at the bottom and scan this code. "
    "Steam changes the code every half minute; the new one shows here.";

} // namespace

std::unique_ptr<tk::Widget> FumaroleQrScreen::build(const tk::Theme& theme) {
    ui::PageFrame<tk::Column> frame = ui::pageFrame<tk::Column>(theme, chrome_);
    frame.body->space(theme.spacing.pad);
    frame.body->add<tk::Label>(title(), tk::TextRole::Title, true);
    tk::Row& row = frame.body->add<tk::Row>();
    row.space(theme.spacing.pad);
    tk::Column& code = row.add<tk::Column>();
    code.fixWidth(theme.spacing.sidebarWidth);
    code.add<ui::ImageView>(images_, signIn_.qr(), 1.0f).whole(tk::Align::Start);
    tk::Column& words = row.add<tk::Column>();
    words.grow();
    words.add<tk::Label>(steps, tk::TextRole::Body, true);
    return std::move(frame.root);
}

tk::Screen::BackReply FumaroleQrScreen::back(tk::ActContext& /*context*/) {
    signIn_.cancel();
    return BackReply::Pop;
}

} // namespace opensu::app
