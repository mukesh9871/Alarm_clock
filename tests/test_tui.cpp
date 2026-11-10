#include "app/tui.hpp"
#include "support/fakes.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>
#include <iostream>

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
    AlarmSession session{file, clock, tz, audio, [](const std::string&) { return std::filesystem::path("x.wav"); }};

    std::string snap() {
        auto s = render_tui_snapshot(session, 90, 30);
        if (std::getenv("ALARM_PRINT_SNAPSHOTS")) std::cout << s << "\n\n";
        return s;
    }
};
}  // namespace

TEST_CASE("TUI shows an empty state with help") {
    Fixture f;
    f.session.start();
    const auto s = f.snap();
    CHECK_THAT(s, ContainsSubstring("No alarms yet"));
    CHECK_THAT(s, ContainsSubstring("Sat 10 Jan 2026"));
    CHECK_THAT(s, ContainsSubstring("add"));
    CHECK_THAT(s, ContainsSubstring("quit"));
}

TEST_CASE("TUI lists alarms with repeat, label and countdown") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 45}, .label = "Gym", .days = DaySet::weekdays()});
    f.session.save_alarm(Alarm{.time = {9, 30}, .label = "Brunch", .enabled = false});
    const auto s = f.snap();
    CHECK_THAT(s, ContainsSubstring("06:45"));
    CHECK_THAT(s, ContainsSubstring("Weekdays"));
    CHECK_THAT(s, ContainsSubstring("Gym"));
    CHECK_THAT(s, ContainsSubstring("[off]"));
    CHECK_THAT(s, ContainsSubstring("Next: 06:45 \"Gym\" in 2d 0h 45m"));  // Sat -> Mon
    CHECK_THAT(s, ContainsSubstring("██"));                      // big clock digits
}

TEST_CASE("TUI shows the ringing overlay") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 1}, .label = "Wake up"});
    f.clock.set(utc(2026, 1, 10, 6, 1));
    f.session.tick();
    const auto s = f.snap();
    CHECK_THAT(s, ContainsSubstring("ALARM"));
    CHECK_THAT(s, ContainsSubstring("Wake up"));
    CHECK_THAT(s, ContainsSubstring("snooze"));
}

TEST_CASE("TUI ringing overlay shows the math challenge") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 1}, .label = "Up", .challenge = ChallengeKind::Math});
    f.clock.set(utc(2026, 1, 10, 6, 1));
    f.session.tick();
    REQUIRE(f.session.challenge() != nullptr);
    const auto s = f.snap();
    CHECK_THAT(s, ContainsSubstring("Solve to stop"));
    CHECK_THAT(s, ContainsSubstring(f.session.challenge()->question));
}

TEST_CASE("TUI lists the other alarms while one rings") {
    Fixture f;
    f.session.start();
    f.session.save_alarm(Alarm{.time = {6, 1}, .label = "Wake up"});
    f.session.save_alarm(Alarm{.time = {9, 30}, .label = "Brunch", .enabled = false});
    f.clock.set(utc(2026, 1, 10, 6, 1));
    f.session.tick();
    // Rows without a next ring time look themselves up in the ringing list
    // (this once compared iterators of two different copies of it).
    const auto s = f.snap();
    CHECK_THAT(s, ContainsSubstring("RINGING"));
    CHECK_THAT(s, ContainsSubstring("[off]"));  // its label is behind the ringing overlay
    CHECK_THAT(s, ContainsSubstring("09:30"));
}
