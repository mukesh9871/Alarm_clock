#include "core/days.hpp"

#include <array>
#include <cctype>

namespace ac {

namespace {

constexpr std::array<std::string_view, 7> kShort{"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
constexpr std::array<std::string_view, 7> kPretty{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

std::string lower_trimmed(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Accepts any prefix of a weekday name with at least two letters ("mo", "tue", "thurs", "friday").
// Two letters are always unambiguous (su/sa, tu/th).
std::optional<unsigned> parse_weekday(std::string_view token) {
    static constexpr std::array<std::string_view, 7> kLong{"sunday",   "monday", "tuesday", "wednesday",
                                                           "thursday", "friday", "saturday"};
    if (token.size() < 2) return std::nullopt;
    for (unsigned i = 0; i < kLong.size(); ++i) {
        if (kLong[i].starts_with(token)) return i;
    }
    return std::nullopt;
}

}  // namespace

std::optional<DaySet> parse_days(std::string_view text) {
    const std::string s = lower_trimmed(text);
    if (s.empty() || s == "once" || s == "never" || s == "none") return DaySet{};
    if (s == "daily" || s == "everyday" || s == "every-day" || s == "all") return DaySet::every_day();
    if (s == "weekdays" || s == "weekday") return DaySet::weekdays();
    if (s == "weekends" || s == "weekend") return DaySet::weekends();

    DaySet days;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t comma = s.find(',', start);
        const std::string_view token = std::string_view(s).substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        const auto wd = parse_weekday(token);
        if (!wd) return std::nullopt;
        days.add(std::chrono::weekday{*wd});
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return days;
}

std::string format_days(DaySet days) {
    if (days.empty()) return "Once";
    if (days == DaySet::every_day()) return "Every day";
    if (days == DaySet::weekdays()) return "Weekdays";
    if (days == DaySet::weekends()) return "Weekends";
    std::string out;
    // Monday-first order reads more naturally.
    for (unsigned i : {1U, 2U, 3U, 4U, 5U, 6U, 0U}) {
        if (days.contains(std::chrono::weekday{i})) {
            if (!out.empty()) out += ' ';
            out += kPretty[i];
        }
    }
    return out;
}

std::string days_to_string(DaySet days) {
    std::string out;
    for (unsigned i : {1U, 2U, 3U, 4U, 5U, 6U, 0U}) {
        if (days.contains(std::chrono::weekday{i})) {
            if (!out.empty()) out += ',';
            out += kShort[i];
        }
    }
    return out;
}

}  // namespace ac
