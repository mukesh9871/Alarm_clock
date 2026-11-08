#include "core/days.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using std::chrono::Friday;
using std::chrono::Monday;
using std::chrono::Saturday;
using std::chrono::Sunday;
using std::chrono::Wednesday;

TEST_CASE("parse_days keywords") {
    CHECK(parse_days("once") == DaySet{});
    CHECK(parse_days("") == DaySet{});
    CHECK(parse_days("daily") == DaySet::every_day());
    CHECK(parse_days("Everyday") == DaySet::every_day());
    CHECK(parse_days("weekdays") == DaySet::weekdays());
    CHECK(parse_days("WEEKENDS") == DaySet::weekends());
}

TEST_CASE("parse_days lists and prefixes") {
    const auto d = parse_days("mon, wed,Friday");
    REQUIRE(d.has_value());
    CHECK(d->contains(Monday));
    CHECK(d->contains(Wednesday));
    CHECK(d->contains(Friday));
    CHECK_FALSE(d->contains(Sunday));

    CHECK(parse_days("sa,su") == DaySet::weekends());
    CHECK(parse_days("tu") == parse_days("tuesday"));
    CHECK(parse_days("th") == parse_days("thurs"));
}

TEST_CASE("parse_days rejects junk") {
    for (const char* bad : {"m", "mon,,tue", "funday", "mon;tue", "monday2", ","}) {
        INFO(bad);
        CHECK_FALSE(parse_days(bad).has_value());
    }
}

TEST_CASE("format_days and days_to_string") {
    CHECK(format_days({}) == "Once");
    CHECK(format_days(DaySet::every_day()) == "Every day");
    CHECK(format_days(DaySet::weekdays()) == "Weekdays");
    CHECK(format_days(DaySet::weekends()) == "Weekends");

    DaySet d;
    d.add(Sunday);
    d.add(Monday);
    d.add(Saturday);
    CHECK(format_days(d) == "Mon Sat Sun");
    CHECK(days_to_string(d) == "mon,sat,sun");
    CHECK(parse_days(days_to_string(d)) == d);  // round trip
}
