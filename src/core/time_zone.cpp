#include "core/time_zone.hpp"

#include <ctime>

namespace ac {

namespace {

std::tm to_tm(const LocalDateTime& l) {
    std::tm tm{};
    tm.tm_year = static_cast<int>(l.date.year()) - 1900;
    tm.tm_mon = static_cast<int>(static_cast<unsigned>(l.date.month())) - 1;
    tm.tm_mday = static_cast<int>(static_cast<unsigned>(l.date.day()));
    tm.tm_hour = l.hour;
    tm.tm_min = l.minute;
    tm.tm_sec = l.second;
    tm.tm_isdst = -1;  // let the C library decide whether DST applies
    return tm;
}

}  // namespace

LocalDateTime SystemTimeZone::to_local(TimePoint t) const {
    const std::time_t tt = t.time_since_epoch().count();
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    using namespace std::chrono;
    return LocalDateTime{
        year_month_day{year{tm.tm_year + 1900}, month{static_cast<unsigned>(tm.tm_mon + 1)},
                       day{static_cast<unsigned>(tm.tm_mday)}},
        tm.tm_hour, tm.tm_min, tm.tm_sec};
}

TimePoint SystemTimeZone::to_utc(const LocalDateTime& local) const {
    std::tm tm = to_tm(local);
    const std::time_t tt = std::mktime(&tm);
    return TimePoint{std::chrono::seconds{tt}};
}

}  // namespace ac
