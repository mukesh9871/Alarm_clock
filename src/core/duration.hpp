#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace ac {

// Parses timer lengths: "10" (minutes), "90s", "10m", "1h30m", "1h 5m 10s", "25:00" (mm:ss),
// "1:30:00" (h:mm:ss). Must be > 0 and at most 24 hours.
[[nodiscard]] std::optional<std::chrono::seconds> parse_duration(std::string_view text);

// "05:09", "1:05:09"
[[nodiscard]] std::string format_hms(std::chrono::seconds d);

// "1:05:09.3" - with tenths, for the stopwatch
[[nodiscard]] std::string format_hms_tenths(std::chrono::milliseconds d);

}  // namespace ac
