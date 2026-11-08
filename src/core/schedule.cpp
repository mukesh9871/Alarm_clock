#include "core/schedule.hpp"

#include <stdexcept>

namespace ac {

TimePoint next_occurrence(TimeOfDay time, TimePoint after, const TimeZone& tz, DaySet days) {
    using namespace std::chrono;
    const LocalDateTime now_local = tz.to_local(after);
    const sys_days today{now_local.date};

    // A full week plus one day covers every weekday even if DST shifts push today's slot out.
    for (int offset = 0; offset <= 8; ++offset) {
        const sys_days day = today + std::chrono::days{offset};
        if (!days.empty() && !days.contains(weekday{day})) continue;
        const TimePoint t = tz.to_utc(LocalDateTime{year_month_day{day}, time.hour, time.minute, 0});
        if (t > after) return t;
    }
    throw std::logic_error("next_occurrence: no occurrence found within 9 days");
}

}  // namespace ac
