# Guide menu, Devices page and quick menu

Status: done except what is unverified below.

Built: Guide opens a left menu (`ui::GuideMenu`, `app::GuideMenuController`), Devices is a page with
Bluetooth, Controllers, Audio output and Display tabs (`app::DevicesController`), and Guide + A
(Ctrl+Tab on a keyboard) opens a right quick menu (`app::QuickMenuController`). The title pill is
hidden over Settings, Devices and the details page by one rule (`ui::Shell::pillTitle`). The
breadcrumb trail covers Devices. See `docs/project-state.md` S023 to S025.

Limits: no PIN or passkey pairing agent; player order is connection order and cannot be changed;
Display has no resolution or mode backend; the Guide opens on button release.

Unverified: real power actions, scan, pairing, the overlay over a real game, mouse clicks on the new
pages, a pad's battery and button test, a backlight.
