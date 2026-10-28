// cli_stores — whether this OS runs the store command-line clients Epic and GOG go through
// (legendary, gogdl): every desktop OS (`cli_stores_desktop.cpp`), not Android
// (`cli_stores_none.cpp`).
#pragma once

namespace opensu::library {

[[nodiscard]] bool cliStoresRun() noexcept;

} // namespace opensu::library
