#include "core/alarm_service.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace ac::test;

namespace {
const TestZone kUtc{minutes{0}};
}

TEST_CASE("AlarmService rings at the scheduled time, not before") {
    AlarmService svc(kUtc);
    const auto t0 = utc(2026, 1, 10, 7, 0);
    const int id = svc.upsert(Alarm{.time = {7, 30}}, t0);

    REQUIRE(svc.next_fire(id) == utc(2026, 1, 10, 7, 30));
    CHECK(svc.tick(utc(2026, 1, 10, 7, 29, 59)).empty());

    const auto events = svc.tick(utc(2026, 1, 10, 7, 30));
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == AlarmEvent::Kind::Ring);
    CHECK(events[0].alarm_id == id);
    CHECK(events[0].late == seconds{0});
    CHECK(svc.ringing() == std::vector<int>{id});

    // Does not re-fire while ringing.
    CHECK(svc.tick(utc(2026, 1, 10, 7, 31)).empty());
}

TEST_CASE("AlarmService still rings if the tick that should have fired was skipped") {
    // The original program compared hour/minute for equality and would miss this.
    AlarmService svc(kUtc);
    svc.upsert(Alarm{.time = {7, 30}}, utc(2026, 1, 10, 7, 0));
    const auto events = svc.tick(utc(2026, 1, 10, 7, 32, 15));  // e.g. machine was briefly suspended
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == AlarmEvent::Kind::Ring);
    CHECK(events[0].late == minutes{2} + seconds{15});
}

TEST_CASE("AlarmService reports alarms missed beyond the grace period instead of ringing") {
    AlarmService svc(kUtc, minutes{30});
    const int id = svc.upsert(Alarm{.time = {7, 30}}, utc(2026, 1, 10, 7, 0));
    const auto events = svc.tick(utc(2026, 1, 10, 9, 0));  // woke from sleep 90 minutes late
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == AlarmEvent::Kind::Missed);
    CHECK(svc.ringing().empty());
    CHECK_FALSE(svc.find(id)->enabled);  // one-shot alarm is consumed
}

TEST_CASE("Dismissing a one-shot alarm disables it") {
    AlarmService svc(kUtc);
    const int id = svc.upsert(Alarm{.time = {7, 30}}, utc(2026, 1, 10, 7, 0));
    svc.tick(utc(2026, 1, 10, 7, 30));
    svc.dismiss(id, utc(2026, 1, 10, 7, 31));
    CHECK(svc.ringing().empty());
    CHECK_FALSE(svc.find(id)->enabled);
    CHECK_FALSE(svc.next_fire(id).has_value());
    CHECK(svc.tick(utc(2026, 1, 11, 7, 30)).empty());
}

TEST_CASE("Disabled alarms never fire; re-enabling re-arms from now") {
    AlarmService svc(kUtc);
    const int id = svc.upsert(Alarm{.time = {7, 30}, .enabled = false}, utc(2026, 1, 10, 7, 0));
    CHECK_FALSE(svc.next_fire(id).has_value());
    CHECK(svc.tick(utc(2026, 1, 10, 8, 0)).empty());

    svc.set_enabled(id, true, utc(2026, 1, 10, 8, 0));
    CHECK(svc.next_fire(id) == utc(2026, 1, 11, 7, 30));
}

TEST_CASE("Multiple alarms: ids, next_due, removal") {
    AlarmService svc(kUtc);
    const auto t0 = utc(2026, 1, 10, 6, 0);
    const int a = svc.upsert(Alarm{.time = {9, 0}}, t0);
    const int b = svc.upsert(Alarm{.time = {7, 0}}, t0);
    CHECK(a != b);
    REQUIRE(svc.next_due().has_value());
    CHECK(svc.next_due()->first == b);

    CHECK(svc.remove(b));
    CHECK_FALSE(svc.remove(b));
    CHECK(svc.next_due()->first == a);
    CHECK(svc.alarms().size() == 1);
}

TEST_CASE("Upserting an existing id replaces it") {
    AlarmService svc(kUtc);
    const auto t0 = utc(2026, 1, 10, 6, 0);
    const int id = svc.upsert(Alarm{.time = {9, 0}, .label = "old"}, t0);
    svc.upsert(Alarm{.id = id, .time = {8, 0}, .label = "new"}, t0);
    REQUIRE(svc.alarms().size() == 1);
    CHECK(svc.find(id)->label == "new");
    CHECK(svc.next_fire(id) == utc(2026, 1, 10, 8, 0));
    // New alarms get fresh ids.
    CHECK(svc.upsert(Alarm{.time = {10, 0}}, t0) == id + 1);
}
