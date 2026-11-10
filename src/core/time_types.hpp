#pragma once

#include <chrono>
#include <compare>

namespace ac {

// An absolute instant (UTC), at one-second resolution.
using TimePoint = std::chrono::sys_seconds;

// A wall-clock time of day, e.g. 07:30.
struct TimeOfDay {
    int hour = 0;    // 0..23
    int minute = 0;  // 0..59

    friend auto operator<=>(const TimeOfDay&, const TimeOfDay&) = default;
};

// A civil (local, time-zone-less) date and time, as shown on a wall clock.
struct LocalDateTime {
    std::chrono::year_month_day date{};
    int hour = 0;
    int minute = 0;
    int second = 0;

    [[nodiscard]] std::chrono::weekday weekday() const { return std::chrono::weekday{std::chrono::sys_days{date}}; }
    [[nodiscard]] TimeOfDay time_of_day() const { return {hour, minute}; }

    friend bool operator==(const LocalDateTime&, const LocalDateTime&) = default;
};

}  // namespace ac
