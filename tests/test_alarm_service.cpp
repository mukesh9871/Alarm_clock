#include "core/alarm_service.hpp"
#include "core/schedule.hpp"
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
    AlarmService svc(kUtc, ServiceConfig{.missed_grace = minutes{30}});
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

TEST_CASE("Repeating alarm only fires on its days and re-arms after dismiss") {
    AlarmService svc(kUtc);
    // 2026-01-10 is a Saturday.
    const int id = svc.upsert(Alarm{.time = {7, 0}, .days = DaySet::weekdays()}, utc(2026, 1, 10, 6, 0));
    CHECK(svc.next_fire(id) == utc(2026, 1, 12, 7, 0));  // Monday

    svc.tick(utc(2026, 1, 12, 7, 0));
    svc.dismiss(id, utc(2026, 1, 12, 7, 1));
    CHECK(svc.find(id)->enabled);  // repeating alarms stay on
    CHECK(svc.next_fire(id) == utc(2026, 1, 13, 7, 0));

    // Friday -> next Monday
    svc.upsert(Alarm{.id = id, .time = {7, 0}, .days = DaySet::weekdays()}, utc(2026, 1, 16, 8, 0));
    CHECK(svc.next_fire(id) == utc(2026, 1, 19, 7, 0));
}

TEST_CASE("next_occurrence with specific days") {
    const auto mwf = *parse_days("mon,wed,fri");
    // Sat 10 Jan -> Mon 12
    CHECK(next_occurrence({6, 0}, utc(2026, 1, 10, 12, 0), kUtc, mwf) == utc(2026, 1, 12, 6, 0));
    // Mon 12 after alarm -> Wed 14
    CHECK(next_occurrence({6, 0}, utc(2026, 1, 12, 6, 0), kUtc, mwf) == utc(2026, 1, 14, 6, 0));
    // Same-day single weekday: Sat 10 at 05:00 with only Saturday -> today; after it -> next Saturday
    const auto sat = *parse_days("sat");
    CHECK(next_occurrence({6, 0}, utc(2026, 1, 10, 5, 0), kUtc, sat) == utc(2026, 1, 10, 6, 0));
    CHECK(next_occurrence({6, 0}, utc(2026, 1, 10, 6, 0), kUtc, sat) == utc(2026, 1, 17, 6, 0));
}

TEST_CASE("Snooze rings again after the snooze interval") {
    AlarmService svc(kUtc);
    const int id = svc.upsert(Alarm{.time = {7, 0}, .snooze_minutes = 9}, utc(2026, 1, 10, 6, 0));
    svc.tick(utc(2026, 1, 10, 7, 0));
    svc.snooze(id, utc(2026, 1, 10, 7, 1));
    CHECK(svc.ringing().empty());
    CHECK(svc.is_snoozed(id));
    CHECK(svc.next_fire(id) == utc(2026, 1, 10, 7, 10));
    CHECK(svc.tick(utc(2026, 1, 10, 7, 9, 59)).empty());

    const auto events = svc.tick(utc(2026, 1, 10, 7, 10));
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == AlarmEvent::Kind::Ring);
    CHECK_FALSE(svc.is_snoozed(id));
}

TEST_CASE("Dismissing while snoozed cancels the snooze") {
    AlarmService svc(kUtc);
    const int id = svc.upsert(Alarm{.time = {7, 0}, .days = DaySet::every_day()}, utc(2026, 1, 10, 6, 0));
    svc.tick(utc(2026, 1, 10, 7, 0));
    svc.snooze(id, utc(2026, 1, 10, 7, 0));
    svc.dismiss(id, utc(2026, 1, 10, 7, 2));
    CHECK_FALSE(svc.is_snoozed(id));
    CHECK(svc.next_fire(id) == utc(2026, 1, 11, 7, 0));
}

TEST_CASE("Unanswered alarm auto-snoozes, then gives up") {
    AlarmService svc(kUtc, ServiceConfig{.ring_timeout = minutes{10}, .max_auto_snoozes = 2});
    const int id = svc.upsert(Alarm{.time = {7, 0}, .snooze_minutes = 5}, utc(2026, 1, 10, 6, 0));

    auto t = utc(2026, 1, 10, 7, 0);
    svc.tick(t);  // ring
    for (int i = 0; i < 2; ++i) {
        t += minutes{10};
        auto ev = svc.tick(t);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].kind == AlarmEvent::Kind::AutoSnoozed);
        t += minutes{5};
        ev = svc.tick(t);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].kind == AlarmEvent::Kind::Ring);
    }
    t += minutes{10};
    const auto ev = svc.tick(t);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].kind == AlarmEvent::Kind::TimedOut);
    CHECK(svc.ringing().empty());
    CHECK_FALSE(svc.find(id)->enabled);
}

TEST_CASE("sync keeps unchanged alarms' runtime state and applies edits") {
    AlarmService svc(kUtc);
    const auto t0 = utc(2026, 1, 10, 6, 0);
    const int a = svc.upsert(Alarm{.time = {7, 0}}, t0);
    const int b = svc.upsert(Alarm{.time = {8, 0}}, t0);
    svc.tick(utc(2026, 1, 10, 7, 0));  // a rings
    REQUIRE(svc.ringing() == std::vector<int>{a});

    auto wanted = svc.alarms();
    // Edit b, keep a unchanged, add c.
    for (auto& al : wanted) {
        if (al.id == b) al.time = {9, 0};
    }
    wanted.push_back(Alarm{.id = 10, .time = {10, 0}});
    svc.sync(wanted, utc(2026, 1, 10, 7, 0, 30));

    CHECK(svc.ringing() == std::vector<int>{a});  // still ringing
    CHECK(svc.next_fire(b) == utc(2026, 1, 10, 9, 0));
    CHECK(svc.next_fire(10) == utc(2026, 1, 10, 10, 0));

    // Removing a ringing alarm stops it.
    std::erase_if(wanted, [&](const Alarm& al) { return al.id == a; });
    svc.sync(wanted, utc(2026, 1, 10, 7, 1));
    CHECK(svc.ringing().empty());
    CHECK(svc.alarms().size() == 2);
}
