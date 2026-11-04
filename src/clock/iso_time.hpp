// iso_time — an ISO 8601 date and time as the stores and their tools write it, read as seconds
// since the Unix epoch.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::clock {

/// "2026-10-29T15:00:00.000Z", a space for the `T`, an offset "+01:00", or no zone for UTC;
/// nothing when it is not one.
[[nodiscard]] std::optional<std::int64_t> parseIsoTime(std::string_view text);

} // namespace opensu::clock
