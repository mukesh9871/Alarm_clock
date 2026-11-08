#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ac {

// The set of weekdays an alarm repeats on. Empty = one-shot alarm.
class DaySet {
public:
    constexpr DaySet() = default;

    static constexpr DaySet from_bits(std::uint8_t bits) { return DaySet{static_cast<std::uint8_t>(bits & 0x7F)}; }
    static constexpr DaySet every_day() { return from_bits(0x7F); }
    static constexpr DaySet weekdays() { return from_bits(0x3E); }  // Mon..Fri
    static constexpr DaySet weekends() { return from_bits(0x41); }  // Sun, Sat

    [[nodiscard]] constexpr bool contains(std::chrono::weekday d) const { return (bits_ >> d.c_encoding()) & 1U; }
    constexpr void add(std::chrono::weekday d) { bits_ = static_cast<std::uint8_t>(bits_ | (1U << d.c_encoding())); }
    [[nodiscard]] constexpr bool empty() const { return bits_ == 0; }
    [[nodiscard]] constexpr std::uint8_t bits() const { return bits_; }

    friend constexpr bool operator==(DaySet, DaySet) = default;

private:
    constexpr explicit DaySet(std::uint8_t bits) : bits_(bits) {}
    std::uint8_t bits_ = 0;
};

// Accepts "once", "daily"/"everyday", "weekdays", "weekends", or a comma list like "mon,wed,fri".
[[nodiscard]] std::optional<DaySet> parse_days(std::string_view text);

// "Once", "Every day", "Weekdays", "Weekends", or "Mon Wed Fri".
[[nodiscard]] std::string format_days(DaySet days);

// "mon,wed,fri" / "" - the machine-readable form used for storage.
[[nodiscard]] std::string days_to_string(DaySet days);

}  // namespace ac
