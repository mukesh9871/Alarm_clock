#pragma once

#include "core/time_types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace ac {

enum class HourFormat { H24, H12 };

// Parses user input such as "7", "07:30", "7:30pm", "12am", "19:05".
// Returns nullopt for anything out of range or malformed.
[[nodiscard]] std::optional<TimeOfDay> parse_time_of_day(std::string_view text);

// "07:30" / "7:30 AM"
[[nodiscard]] std::string format_time_of_day(TimeOfDay t, HourFormat fmt = HourFormat::H24);

// "14:03:07" / "2:03:07 PM"
[[nodiscard]] std::string format_clock(const LocalDateTime& t, HourFormat fmt = HourFormat::H24);

// "Sun 08 Nov 2026" (locale-independent)
[[nodiscard]] std::string format_date(const LocalDateTime& t);

// "9h 12m", "45m", "under a minute"
[[nodiscard]] std::string format_duration(std::chrono::seconds d);

}  // namespace ac
