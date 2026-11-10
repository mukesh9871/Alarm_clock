#include "core/schedule.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace ac::test;

TEST_CASE("next_occurrence: later today") {
    const TestZone tz{minutes{0}};
    CHECK(next_occurrence({7, 30}, utc(2026, 1, 10, 6, 0), tz) == utc(2026, 1, 10, 7, 30));
}

TEST_CASE("next_occurrence: already passed today rolls to tomorrow") {
    const TestZone tz{minutes{0}};
    CHECK(next_occurrence({7, 30}, utc(2026, 1, 10, 8, 0), tz) == utc(2026, 1, 11, 7, 30));
}

TEST_CASE("next_occurrence: exactly now is not 'next'") {
    const TestZone tz{minutes{0}};
    CHECK(next_occurrence({7, 30}, utc(2026, 1, 10, 7, 30), tz) == utc(2026, 1, 11, 7, 30));
    CHECK(next_occurrence({7, 30}, utc(2026, 1, 10, 7, 29, 59), tz) == utc(2026, 1, 10, 7, 30));
}

TEST_CASE("next_occurrence: midnight, month and year rollover") {
    const TestZone tz{minutes{0}};
    CHECK(next_occurrence({0, 0}, utc(2026, 1, 31, 23, 59), tz) == utc(2026, 2, 1, 0, 0));
    CHECK(next_occurrence({6, 0}, utc(2026, 12, 31, 22, 0), tz) == utc(2027, 1, 1, 6, 0));
    CHECK(next_occurrence({6, 0}, utc(2028, 2, 28, 7, 0), tz) == utc(2028, 2, 29, 6, 0));  // leap day
}

TEST_CASE("next_occurrence: respects the zone offset") {
    const TestZone ist{minutes{330}};  // UTC+05:30
    // 07:00 IST == 01:30 UTC
    CHECK(next_occurrence({7, 0}, utc(2026, 1, 10, 0, 0), ist) == utc(2026, 1, 10, 1, 30));
    // Local date already rolled over even though UTC date has not
    CHECK(next_occurrence({7, 0}, utc(2026, 1, 10, 20, 0), ist) == utc(2026, 1, 11, 1, 30));
}

TEST_CASE("next_occurrence: DST spring forward") {
    const auto tz = eastern_2026();
    SECTION("normal time on the transition day uses the new offset") {
        // 07:00 EDT (UTC-4) on Mar 8 == 11:00 UTC
        CHECK(next_occurrence({7, 0}, utc(2026, 3, 8, 5, 0), tz) == utc(2026, 3, 8, 11, 0));
    }
    SECTION("a skipped wall time fires at the shifted instant instead of being lost") {
        // 02:30 does not exist on Mar 8; it fires at 03:30 EDT == 07:30 UTC
        const auto t = next_occurrence({2, 30}, utc(2026, 3, 8, 5, 0), tz);
        CHECK(t == utc(2026, 3, 8, 7, 30));
        CHECK(tz.to_local(t) == local(2026, 3, 8, 3, 30));
    }
}

TEST_CASE("next_occurrence: DST fall back fires once") {
    const auto tz = eastern_2026();
    // 01:30 happens twice on Nov 1 (EDT at 05:30 UTC, then EST at 06:30 UTC).
    const auto first = next_occurrence({1, 30}, utc(2026, 11, 1, 0, 0), tz);
    CHECK(first == utc(2026, 11, 1, 5, 30));
    // After it fires, the repeated 01:30 must not fire again the same night.
    const auto second = next_occurrence({1, 30}, first, tz);
    CHECK(second == utc(2026, 11, 2, 6, 30));
}
