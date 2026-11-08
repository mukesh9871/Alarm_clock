#include "core/duration.hpp"

#include <cctype>
#include <cstdio>
#include <vector>

namespace ac {

namespace {

constexpr long long kMaxSeconds = 24LL * 3600;

std::optional<std::chrono::seconds> checked(long long s) {
    if (s <= 0 || s > kMaxSeconds) return std::nullopt;
    return std::chrono::seconds{s};
}

// "25:00" or "1:30:00"
std::optional<std::chrono::seconds> parse_clock_style(std::string_view s) {
    std::vector<long long> parts{0};
    std::vector<std::size_t> digits{0};
    for (char c : s) {
        if (c == ':') {
            parts.push_back(0);
            digits.push_back(0);
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            if (++digits.back() > 4) return std::nullopt;
            parts.back() = parts.back() * 10 + (c - '0');
        } else {
            return std::nullopt;
        }
    }
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (digits[i] == 0) return std::nullopt;
        if (i > 0 && (digits[i] != 2 || parts[i] > 59)) return std::nullopt;  // :MM and :SS are two digits
    }
    if (parts.size() == 2) return checked(parts[0] * 60 + parts[1]);
    if (parts.size() == 3) return checked(parts[0] * 3600 + parts[1] * 60 + parts[2]);
    return std::nullopt;
}

}  // namespace

std::optional<std::chrono::seconds> parse_duration(std::string_view text) {
    std::string s;
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s.empty()) return std::nullopt;
    if (s.find(':') != std::string::npos) return parse_clock_style(s);

    long long total = 0;
    long long number = 0;
    std::size_t digits = 0;
    bool any_unit = false;
    int last_unit_rank = 4;  // enforce h > m > s order, each at most once
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            if (++digits > 6) return std::nullopt;
            number = number * 10 + (c - '0');
            continue;
        }
        if (digits == 0) return std::nullopt;
        int rank = 0;
        long long scale = 0;
        if (c == 'h') {
            rank = 3;
            scale = 3600;
        } else if (c == 'm') {
            rank = 2;
            scale = 60;
        } else if (c == 's') {
            rank = 1;
            scale = 1;
        } else {
            return std::nullopt;
        }
        if (rank >= last_unit_rank) return std::nullopt;
        last_unit_rank = rank;
        total += number * scale;
        number = 0;
        digits = 0;
        any_unit = true;
    }
    if (digits > 0) {
        if (any_unit) return std::nullopt;  // "1h30" is ambiguous
        total = number * 60;                // bare number = minutes
    }
    return checked(total);
}

std::string format_hms(std::chrono::seconds d) {
    const long long total = d.count() < 0 ? 0 : d.count();
    const long long h = total / 3600, m = (total / 60) % 60, s = total % 60;
    char buf[32];
    if (h > 0) {
        std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", h, m, s);
    } else {
        std::snprintf(buf, sizeof buf, "%02lld:%02lld", m, s);
    }
    return buf;
}

std::string format_hms_tenths(std::chrono::milliseconds d) {
    const long long ms = d.count() < 0 ? 0 : d.count();
    return format_hms(std::chrono::seconds{ms / 1000}) + "." + std::to_string((ms / 100) % 10);
}

}  // namespace ac
