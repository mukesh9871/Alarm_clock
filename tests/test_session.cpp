#include "app/session.hpp"
#include "support/fakes.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>

using namespace ac;
using namespace ac::test;
using Catch::Matchers::ContainsSubstring;

namespace {

std::filesystem::path test_sound(const std::string& s) { return s.empty() ? "default.wav" : s; }

struct Fixture {
    TempDir dir;
    StateFile file{dir.path() / "alarms.json"};
    FakeClock clock{utc(2026, 1, 10, 6, 0)};
    TestZone tz{minutes{0}};
    FakeAudio audio;
    AlarmSession session{file, clock, tz, audio, test_sound};
    // The same app started again later, on the same alarm file.
    StateFile reopened_file{dir.path() / "alarms.json"};
    AlarmSession reopened{reopened_file, clock, tz, audio, test_sound};

    void write_state(const SavedState& s) { StateFile(file.path()).save(s); }
    SavedState read_state() { return StateFile(file.path()).load().state; }
    std::string raw_file() {
        std::ifstream in(file.path(), std::ios::binary);
        std::ostringstream buf;
        buf << in.rdbuf();
        return buf.str();
    }
};

const Alarm* find_alarm(const std::vector<Alarm>& alarms, int id) {
    for (const auto& a : alarms) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

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

TEST_CASE("Math challenge must be solved to stop, but snooze still works") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}, .snooze_minutes = 5, .challenge = ChallengeKind::Math});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    f.session.tick();

    const MathProblem* p = f.session.challenge();
    REQUIRE(p != nullptr);
    CHECK(f.session.dismiss() == AlarmSession::DismissResult::NeedsAnswer);
    CHECK(f.audio.playing);

    const std::string first = p->question;
    const int wrong = p->answer + 1;
    CHECK(f.session.dismiss(wrong) == AlarmSession::DismissResult::WrongAnswer);
    CHECK(f.audio.playing);
    REQUIRE(f.session.challenge() != nullptr);

    // Snoozing is allowed without solving.
    f.session.snooze();
    CHECK_FALSE(f.audio.playing);
    CHECK(f.session.challenge() == nullptr);

    f.clock.set(utc(2026, 1, 10, 6, 35));
    f.session.tick();
    REQUIRE(f.session.challenge() != nullptr);
    CHECK(f.session.dismiss(f.session.challenge()->answer) == AlarmSession::DismissResult::Dismissed);
    CHECK_FALSE(f.audio.playing);
    CHECK(f.session.challenge() == nullptr);
    CHECK(f.session.dismiss() == AlarmSession::DismissResult::NotRinging);
}

TEST_CASE("Alarms without a challenge stop immediately") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    f.session.tick();
    CHECK(f.session.challenge() == nullptr);
    CHECK(f.session.dismiss() == AlarmSession::DismissResult::Dismissed);
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

namespace {
class FakePower final : public PowerManager {
public:
    bool schedule_wake(std::optional<TimePoint> when) override {
        wake = when;
        return accept;
    }
    void keep_awake(bool on) override { awake = on; }
    bool wake_supported() const override { return true; }

    std::optional<TimePoint> wake;
    bool awake = false;
    bool accept = true;
};

struct PowerFixture {
    TempDir dir;
    StateFile file{dir.path() / "alarms.json"};
    FakeClock clock{utc(2026, 1, 10, 6, 0)};
    TestZone tz{minutes{0}};
    FakeAudio audio;
    FakePower power;
    AlarmSession session{file, clock, tz, audio, [](const std::string&) { return std::filesystem::path("x.wav"); },
                         &power};
};
}  // namespace

TEST_CASE("Session wakes the computer before the next alarm and keeps it awake while ringing") {
    PowerFixture f;
    f.session.start();
    CHECK_FALSE(f.power.wake.has_value());

    f.session.save_alarm(Alarm{.time = {7, 0}, .snooze_minutes = 5});
    CHECK(f.power.wake == utc(2026, 1, 10, 6, 59));  // one minute early
    CHECK(f.session.wake_timer_ok());

    // Within the lead time, wake at the alarm itself rather than in the past.
    f.clock.set(utc(2026, 1, 10, 6, 59, 30));
    f.session.tick();
    CHECK(f.power.wake == utc(2026, 1, 10, 7, 0));

    f.clock.set(utc(2026, 1, 10, 7, 0));
    f.session.tick();
    CHECK(f.power.awake);
    f.session.snooze();
    CHECK_FALSE(f.power.awake);
    CHECK(f.power.wake == utc(2026, 1, 10, 7, 4));  // snooze ends 07:05, wake 1 min early

    f.session.shutdown();
    CHECK_FALSE(f.power.wake.has_value());  // app closed: nothing to ring, so don't wake
}

TEST_CASE("Session reports when the OS refuses the wake timer") {
    PowerFixture f;
    f.power.accept = false;
    f.session.start();
    CHECK(f.session.wake_timer_ok());  // nothing to schedule yet
    f.session.save_alarm(Alarm{.time = {7, 0}});
    CHECK_FALSE(f.session.wake_timer_ok());
}

TEST_CASE("Session keeps its alarms when a reload finds the file unreadable") {
    Fixture f;
    f.session.start();
    const int id = f.session.save_alarm(Alarm{.time = {6, 30}});

    // Caught mid-write by an editor, or broken by hand.
    {
        std::ofstream(f.file.path(), std::ios::binary | std::ios::trunc) << "{ half-written";
    }
    bump_mtime(f.file.path());
    f.clock.advance(std::chrono::seconds{1});
    CHECK(has_notice(f.session.tick(), Notice::Level::Warning, "Could not read"));
    CHECK(f.session.service().find(id) != nullptr);
    CHECK(f.raw_file() == "{ half-written");  // left for the user, not replaced or moved
    CHECK_FALSE(std::filesystem::exists(f.dir.path() / "alarms.json.corrupt-1"));

    // The alarm still rings.
    f.clock.set(utc(2026, 1, 10, 6, 30));
    CHECK(has_notice(f.session.tick(), Notice::Level::Alarm, "06:30"));
    f.session.dismiss();
    CHECK(f.raw_file() == "{ half-written");

    // Once the file is valid again, the session follows it.
    SavedState fixed;
    fixed.alarms.push_back(Alarm{.id = 7, .time = {8, 0}});
    f.write_state(fixed);
    bump_mtime(f.file.path());
    CHECK(has_notice(f.session.tick(), Notice::Level::Info, "reloaded"));
    CHECK(f.session.service().find(7) != nullptr);
    CHECK(f.session.service().find(id) == nullptr);
}

TEST_CASE("Session edits don't overwrite changes made from another process") {
    Fixture f;
    f.session.start();
    const int mine = f.session.save_alarm(Alarm{.time = {6, 30}});

    SavedState s = f.read_state();
    s.alarms.push_back(Alarm{.id = 2, .time = {7, 0}, .label = "from CLI"});
    f.write_state(s);
    bump_mtime(f.file.path());

    // The user acts in the UI before the next tick has reloaded the file.
    f.session.toggle_alarm(mine);
    const auto saved = f.read_state().alarms;
    REQUIRE(saved.size() == 2);
    CHECK_FALSE(find_alarm(saved, mine)->enabled);
    REQUIRE(find_alarm(saved, 2) != nullptr);
    CHECK(find_alarm(saved, 2)->label == "from CLI");

    // A new UI alarm doesn't reuse the CLI's id, and the reload is reported on the next tick.
    CHECK(f.session.save_alarm(Alarm{.time = {9, 0}}) == 3);
    CHECK(has_notice(f.session.tick(), Notice::Level::Info, "reloaded"));
}

TEST_CASE("A snooze survives closing and reopening the app") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}, .snooze_minutes = 5});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    f.session.tick();
    f.session.snooze();
    f.clock.set(utc(2026, 1, 10, 6, 31));
    f.session.shutdown();
    CHECK(f.read_state().pending == std::vector<PendingRing>{{1, utc(2026, 1, 10, 6, 35)}});

    f.clock.set(utc(2026, 1, 10, 6, 33));
    CHECK(f.reopened.start().empty());
    CHECK(f.reopened.service().is_snoozed(1));
    CHECK(f.reopened.service().next_fire(1) == utc(2026, 1, 10, 6, 35));

    f.clock.set(utc(2026, 1, 10, 6, 35));
    CHECK(has_notice(f.reopened.tick(), Notice::Level::Alarm, "06:30"));
    f.reopened.dismiss();
    const auto state = f.read_state();
    CHECK_FALSE(state.alarms.at(0).enabled);  // the one-shot is done, not re-armed for tomorrow
    CHECK(state.pending.empty());
}

TEST_CASE("An alarm ringing when the app died rings again on restart, or is reported missed") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 30}, .label = "Once"});
    f.clock.set(utc(2026, 1, 10, 6, 30));
    f.session.tick();  // ringing - and then the process is killed (no shutdown)
    REQUIRE(f.read_state().pending.size() == 1);

    SECTION("restarted within the grace period") {
        f.clock.set(utc(2026, 1, 10, 6, 40));
        CHECK(has_notice(f.reopened.start(), Notice::Level::Alarm, "Once"));
    }
    SECTION("restarted much later") {
        f.clock.set(utc(2026, 1, 10, 9, 0));
        CHECK(has_notice(f.reopened.start(), Notice::Level::Warning, "Missed alarm 06:30"));
        CHECK_FALSE(f.read_state().alarms.at(0).enabled);
        CHECK(f.read_state().pending.empty());
    }
}
