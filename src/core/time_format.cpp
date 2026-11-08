#include "core/time_format.hpp"

#include <array>
#include <cctype>
#include <cstdio>

namespace ac {

namespace {

constexpr std::array<const char*, 7> kWeekdays{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr std::array<const char*, 12> kMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

// Reads 1-2 digits; returns -1 if none.
int read_number(std::string_view& s, std::size_t max_digits) {
    int value = 0;
    std::size_t n = 0;
    while (n < max_digits && n < s.size() && std::isdigit(static_cast<unsigned char>(s[n]))) {
        value = value * 10 + (s[n] - '0');
        ++n;
    }
    if (n == 0) return -1;
    s.remove_prefix(n);
    return value;
}

int to_12h(int hour) {
    const int h = hour % 12;
    return h == 0 ? 12 : h;
}

}  // namespace

std::optional<TimeOfDay> parse_time_of_day(std::string_view text) {
    std::string_view s = trim(text);

    const int hour = read_number(s, 2);
    if (hour < 0) return std::nullopt;

    int minute = 0;
    if (!s.empty() && (s.front() == ':' || s.front() == '.')) {
        s.remove_prefix(1);
        if (s.size() < 2 || !std::isdigit(static_cast<unsigned char>(s[0])) ||
            !std::isdigit(static_cast<unsigned char>(s[1]))) {
            return std::nullopt;  // require exactly two minute digits
        }
        minute = read_number(s, 2);
    }

    s = trim(s);
    enum class Suffix { None, Am, Pm } suffix = Suffix::None;
    if (!s.empty()) {
        std::string lower;
        for (char c : s) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == "am" || lower == "a" || lower == "a.m.") {
            suffix = Suffix::Am;
        } else if (lower == "pm" || lower == "p" || lower == "p.m.") {
            suffix = Suffix::Pm;
        } else {
            return std::nullopt;
        }
    }

    if (minute > 59) return std::nullopt;

    int h24 = hour;
    if (suffix == Suffix::None) {
        if (hour > 23) return std::nullopt;
    } else {
        if (hour < 1 || hour > 12) return std::nullopt;
        h24 = hour % 12 + (suffix == Suffix::Pm ? 12 : 0);
    }
    return TimeOfDay{h24, minute};
}

std::string format_time_of_day(TimeOfDay t, HourFormat fmt) {
    char buf[16];
    if (fmt == HourFormat::H24) {
        std::snprintf(buf, sizeof buf, "%02d:%02d", t.hour, t.minute);
    } else {
        std::snprintf(buf, sizeof buf, "%d:%02d %s", to_12h(t.hour), t.minute, t.hour < 12 ? "AM" : "PM");
    }
    return buf;
}

std::string format_clock(const LocalDateTime& t, HourFormat fmt) {
    char buf[24];
    if (fmt == HourFormat::H24) {
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", t.hour, t.minute, t.second);
    } else {
        std::snprintf(buf, sizeof buf, "%d:%02d:%02d %s", to_12h(t.hour), t.minute, t.second,
                      t.hour < 12 ? "AM" : "PM");
    }
    return buf;
}

std::string format_date(const LocalDateTime& t) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%s %02u %s %d", kWeekdays[t.weekday().c_encoding()],
                  static_cast<unsigned>(t.date.day()), kMonths[static_cast<unsigned>(t.date.month()) - 1],
                  static_cast<int>(t.date.year()));
    return buf;
}

std::string format_duration(std::chrono::seconds d) {
    using namespace std::chrono;
    if (d < minutes{1}) return "under a minute";
    const auto total_min = duration_cast<minutes>(d).count();
    const auto days = total_min / (24 * 60);
    const auto hours = (total_min / 60) % 24;
    const auto mins = total_min % 60;

    std::string out;
    if (days > 0) out += std::to_string(days) + "d ";
    if (days > 0 || hours > 0) out += std::to_string(hours) + "h ";
    out += std::to_string(mins) + "m";
    return out;
}

}  // namespace ac
