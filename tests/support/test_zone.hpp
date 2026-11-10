#pragma once

#include "core/time_zone.hpp"

#include <chrono>
#include <optional>

namespace ac::test {

using namespace std::chrono;

inline TimePoint utc(int y, unsigned mo, unsigned d, int h = 0, int mi = 0, int s = 0) {
    return TimePoint{sys_days{year{y} / month{mo} / day{d}}} + hours{h} + minutes{mi} + seconds{s};
}

inline LocalDateTime local(int y, unsigned mo, unsigned d, int h = 0, int mi = 0, int s = 0) {
    return LocalDateTime{year{y} / month{mo} / day{d}, h, mi, s};
}

// A deterministic zone with a fixed base offset and an optional one-hour DST window
// [dst_start, dst_end) in UTC. Lets tests exercise DST without touching the OS zone.
class TestZone final : public TimeZone {
public:
    explicit TestZone(minutes base, std::optional<TimePoint> dst_start = {}, std::optional<TimePoint> dst_end = {})
        : base_(base), dst_start_(dst_start), dst_end_(dst_end) {}

    LocalDateTime to_local(TimePoint t) const override {
        const TimePoint wall = t + offset_at(t);
        const sys_days day = floor<days>(wall);
        const auto tod = hh_mm_ss<seconds>{wall - day};
        return LocalDateTime{year_month_day{day}, static_cast<int>(tod.hours().count()),
                             static_cast<int>(tod.minutes().count()), static_cast<int>(tod.seconds().count())};
    }

    TimePoint to_utc(const LocalDateTime& l) const override {
        const TimePoint naive =
            TimePoint{sys_days{l.date}} + hours{l.hour} + minutes{l.minute} + seconds{l.second};
        const TimePoint as_dst = naive - base_ - hours{1};
        const TimePoint as_std = naive - base_;
        // Earlier candidate first so ambiguous times resolve to the first occurrence.
        if (to_local(as_dst) == l) return as_dst;
        if (to_local(as_std) == l) return as_std;
        return as_std;  // in the spring-forward gap: shift forward
    }

private:
    minutes offset_at(TimePoint t) const {
        const bool dst = dst_start_ && dst_end_ && t >= *dst_start_ && t < *dst_end_;
        return base_ + (dst ? minutes{60} : minutes{0});
    }

    minutes base_;
    std::optional<TimePoint> dst_start_, dst_end_;
};

// A US-Eastern-like zone for 2026: UTC-5, DST (UTC-4) from Mar 8 02:00 local to Nov 1 02:00 local.
inline TestZone eastern_2026() {
    return TestZone{minutes{-300}, utc(2026, 3, 8, 7), utc(2026, 11, 1, 6)};
}

}  // namespace ac::test
