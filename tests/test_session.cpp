#include "app/session.hpp"
#include "support/fakes.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace ac;
using namespace ac::test;
using Catch::Matchers::ContainsSubstring;

namespace {

struct Fixture {
    TempDir dir;
    StateFile file{dir.path() / "alarms.json"};
    FakeClock clock{utc(2026, 1, 10, 6, 0)};
    TestZone tz{minutes{0}};
    FakeAudio audio;
    AlarmSession session{file, clock, tz, audio, [](const std::string& s) { return std::filesystem::path(s.empty() ? "default.wav" : s); }};

    void write_state(const SavedState& s) { StateFile(file.path()).save(s); }
    SavedState read_state() { return StateFile(file.path()).load().state; }
};

bool has_notice(const std::vector<Notice>& ns, Notice::Level level, const std::string& text) {
    for (const auto& n : ns) {
        if (n.level == level && n.text.find(text) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("Session rings, plays the alarm's sound with its fade, and stops on dismiss") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}, .label = "Wake", .sound = "bell.mp3", .fade_in_seconds = 20});

    f.clock.set(utc(2026, 1, 10, 6, 30));
    const auto notices = f.session.tick();
    CHECK(has_notice(notices, Notice::Level::Alarm, "Wake"));
    REQUIRE(f.audio.started.size() == 1);
    CHECK(f.audio.started[0] == "bell.mp3");
    CHECK(f.audio.last_fade == std::chrono::seconds{20});
    CHECK(f.audio.playing);

    f.session.dismiss();
    CHECK_FALSE(f.audio.playing);
    // One-shot alarm is now disabled - and that was saved.
    CHECK_FALSE(f.read_state().alarms.at(0).enabled);
}

TEST_CASE("Session snooze stops audio and rings again later") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}, .snooze_minutes = 5});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    f.session.tick();
    f.session.snooze();
    CHECK_FALSE(f.audio.playing);

    f.clock.set(utc(2026, 1, 10, 6, 34, 59));
    CHECK(f.session.tick().empty());
    f.clock.set(utc(2026, 1, 10, 6, 35));
    CHECK(has_notice(f.session.tick(), Notice::Level::Alarm, "06:30"));
    CHECK(f.audio.playing);
}

TEST_CASE("Session reports alarms missed while the app was closed") {
    Fixture f;
    SavedState s;
    s.alarms.push_back(Alarm{.id = 1, .time = {7, 0}, .label = "Standup", .days = DaySet::every_day()});
    s.last_seen = utc(2026, 1, 10, 6, 0);  // app closed at 06:00
    f.write_state(s);

    f.clock.set(utc(2026, 1, 10, 9, 0));  // reopened at 09:00
    const auto notices = f.session.start();
    CHECK(has_notice(notices, Notice::Level::Warning, "Missed alarm 07:00 \"Standup\""));
    CHECK(f.audio.started.empty());
    // Repeating alarm stays enabled and is armed for tomorrow.
    CHECK(f.session.service().next_fire(1) == utc(2026, 1, 11, 7, 0));
}

TEST_CASE("Session rings an alarm that came due just before it started") {
    Fixture f;
    SavedState s;
    s.alarms.push_back(Alarm{.id = 1, .time = {7, 0}});
    s.last_seen = utc(2026, 1, 10, 6, 55);
    f.write_state(s);

    f.clock.set(utc(2026, 1, 10, 7, 5));  // restarted 5 minutes late - within grace
    CHECK(has_notice(f.session.start(), Notice::Level::Alarm, "07:00"));
}

TEST_CASE("Session does not report alarms created after it last ran as missed") {
    Fixture f;
    SavedState s;
    // Added via the CLI at 08:00 while the clock wasn't running; 07:00 meant "tomorrow".
    s.alarms.push_back(Alarm{.id = 1, .time = {7, 0}, .updated_at = utc(2026, 1, 10, 8, 0)});
    s.last_seen = utc(2026, 1, 9, 22, 0);
    f.write_state(s);

    f.clock.set(utc(2026, 1, 10, 8, 30));
    CHECK(f.session.start().empty());
    CHECK(f.session.service().next_fire(1) == utc(2026, 1, 11, 7, 0));
}

TEST_CASE("Session picks up alarms added from another process") {
    Fixture f;
    f.session.start();
    CHECK_FALSE(f.session.service().next_due().has_value());

    SavedState s = f.read_state();
    s.alarms.push_back(Alarm{.id = 1, .time = {6, 10}});
    f.write_state(s);
    std::filesystem::last_write_time(f.file.path(), std::filesystem::file_time_type::clock::now() + std::chrono::seconds{5});

    f.clock.advance(std::chrono::seconds{1});
    CHECK(has_notice(f.session.tick(), Notice::Level::Info, "reloaded"));
    CHECK(f.session.service().next_fire(1) == utc(2026, 1, 10, 6, 10));
}

TEST_CASE("Session falls back to a beep warning when the sound can't play") {
    Fixture f;
    f.audio.succeed = false;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    CHECK(has_notice(f.session.tick(), Notice::Level::Warning, "beeping"));
}

TEST_CASE("Session records last_seen periodically") {
    Fixture f;
    f.session.start();
    CHECK(f.read_state().last_seen == utc(2026, 1, 10, 6, 0));
    f.clock.advance(std::chrono::seconds{30});
    f.session.tick();
    CHECK(f.read_state().last_seen == utc(2026, 1, 10, 6, 0));  // not yet
    f.clock.advance(std::chrono::seconds{30});
    f.session.tick();
    CHECK(f.read_state().last_seen == utc(2026, 1, 10, 6, 1));
}

TEST_CASE("Session hour-format setting persists") {
    Fixture f;
    f.session.start();
    f.session.set_hour_format(HourFormat::H12);
    CHECK(f.read_state().settings.hour_format == HourFormat::H12);
    const int id = f.session.save_alarm(Alarm{.time = {19, 5}});
    CHECK(f.session.describe(id) == "7:05 PM");
}
