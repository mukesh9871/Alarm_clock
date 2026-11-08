#include "core/schedule.hpp"

#include <stdexcept>

namespace ac {

TimePoint next_occurrence(TimeOfDay time, TimePoint after, const TimeZone& tz) {
    using namespace std::chrono;
    const LocalDateTime now_local = tz.to_local(after);
    const sys_days today{now_local.date};

    // Look one day further than strictly necessary so DST shifts can't push us past the window.
    for (int offset = 0; offset <= 2; ++offset) {
        const LocalDateTime candidate{year_month_day{today + days{offset}}, time.hour, time.minute, 0};
        const TimePoint t = tz.to_utc(candidate);
        if (t > after) return t;
    }
    throw std::logic_error("next_occurrence: no occurrence found within 3 days");
}

}  // namespace ac
