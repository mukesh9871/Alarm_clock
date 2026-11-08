#include "core/time_format.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace std::chrono;

TEST_CASE("parse_time_of_day accepts common formats") {
    CHECK(parse_time_of_day("07:30") == TimeOfDay{7, 30});
    CHECK(parse_time_of_day("7:30") == TimeOfDay{7, 30});
    CHECK(parse_time_of_day("  19:05 ") == TimeOfDay{19, 5});
    CHECK(parse_time_of_day("0:00") == TimeOfDay{0, 0});
    CHECK(parse_time_of_day("23:59") == TimeOfDay{23, 59});
    CHECK(parse_time_of_day("7") == TimeOfDay{7, 0});
    CHECK(parse_time_of_day("7.15") == TimeOfDay{7, 15});
}

TEST_CASE("parse_time_of_day handles am/pm") {
    CHECK(parse_time_of_day("7:30pm") == TimeOfDay{19, 30});
    CHECK(parse_time_of_day("7:30 PM") == TimeOfDay{19, 30});
    CHECK(parse_time_of_day("7am") == TimeOfDay{7, 0});
    CHECK(parse_time_of_day("12am") == TimeOfDay{0, 0});
    CHECK(parse_time_of_day("12:15pm") == TimeOfDay{12, 15});
    CHECK(parse_time_of_day("6 p") == TimeOfDay{18, 0});
}

TEST_CASE("parse_time_of_day rejects invalid input") {
    for (const char* bad : {"", "   ", "abc", "24:00", "25", "7:60", "7:5", "7:", ":30", "13pm", "0am",
                            "7:30xm", "123", "7:30:00", "-1", "7 30"}) {
        INFO("input: '" << bad << "'");
        CHECK_FALSE(parse_time_of_day(bad).has_value());
    }
}

TEST_CASE("format_time_of_day and format_clock pad correctly") {
    CHECK(format_time_of_day({7, 5}) == "07:05");
    CHECK(format_time_of_day({0, 0}, HourFormat::H12) == "12:00 AM");
    CHECK(format_time_of_day({12, 30}, HourFormat::H12) == "12:30 PM");
    CHECK(format_time_of_day({19, 5}, HourFormat::H12) == "7:05 PM");

    const auto t = ac::test::local(2026, 11, 8, 6, 4, 10);
    CHECK(format_clock(t) == "06:04:10");  // the original printed "6:4:10"
    CHECK(format_clock(t, HourFormat::H12) == "6:04:10 AM");
    CHECK(format_date(t) == "Sun 08 Nov 2026");
}

TEST_CASE("format_duration") {
    CHECK(format_duration(seconds{30}) == "under a minute");
    CHECK(format_duration(minutes{45}) == "45m");
    CHECK(format_duration(hours{9} + minutes{12} + seconds{59}) == "9h 12m");
    CHECK(format_duration(hours{2}) == "2h 0m");
    CHECK(format_duration(hours{50}) == "2d 2h 0m");
}
