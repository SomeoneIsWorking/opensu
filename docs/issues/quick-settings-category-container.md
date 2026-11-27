# Quick settings wraps a tab's categories in a box Customize is not in

Status: fixed. A tab's categories and its Customize row sit straight in the panel's rows, one width (`quick_settings_panel.cpp:buildGame`, `buildUse`); seen on the Ally over The Smurfs – Dreams, 2026-11-27.

Reported by the player (2026-11-27, Ally, Quick settings › Game over The Smurfs – Dreams): "this
Customize here is larger than other items, which is, you know, not aesthetic, I don't think we need
the extra container in categories". The category rows (Frame generation, Resolution and upscaling)
sit inside a rounded container, so they are narrower than the Customize row below it.
