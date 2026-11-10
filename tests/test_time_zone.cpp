#include "core/time_zone.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace ac::test;

TEST_CASE("SystemTimeZone round-trips a local time") {
    const SystemTimeZone tz;
    // Mid-day in mid-January avoids DST transitions in every real-world zone.
    const auto l = local(2026, 1, 15, 12, 34, 56);
    CHECK(tz.to_local(tz.to_utc(l)) == l);
}

TEST_CASE("SystemTimeZone agrees with the C library for now") {
    const SystemTimeZone tz;
    const auto now = floor<seconds>(system_clock::now());
    const auto l = tz.to_local(now);
    CHECK(tz.to_utc(l) == now);
}

TEST_CASE("TestZone models DST transitions") {
    const auto tz = eastern_2026();
    CHECK(tz.to_local(utc(2026, 1, 1, 12)) == local(2026, 1, 1, 7));   // EST
    CHECK(tz.to_local(utc(2026, 7, 1, 12)) == local(2026, 7, 1, 8));   // EDT
    CHECK(tz.to_local(utc(2026, 3, 8, 6, 59)) == local(2026, 3, 8, 1, 59));
    CHECK(tz.to_local(utc(2026, 3, 8, 7, 0)) == local(2026, 3, 8, 3, 0));  // 02:00 skipped
}
