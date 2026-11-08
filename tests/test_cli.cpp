#include "app/cli.hpp"
#include "app/commands.hpp"
#include "support/fakes.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>

using namespace ac;
using namespace ac::test;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("cli::parse: no arguments runs the clock") {
    const auto inv = cli::parse({});
    CHECK(std::holds_alternative<cli::Run>(inv.command));
    CHECK_FALSE(inv.data_file.has_value());
}

TEST_CASE("cli::parse: add with options") {
    const auto inv = cli::parse({"--data", "x.json", "add", "6:45am", "--days", "weekdays", "-l", "Gym", "--snooze",
                                 "5", "--fade", "60", "--sound", "bell.wav"});
    CHECK(inv.data_file == std::filesystem::path("x.json"));
    const auto& add = std::get<cli::Add>(inv.command);
    CHECK(add.options.time == TimeOfDay{6, 45});
    CHECK(add.options.days == DaySet::weekdays());
    CHECK(add.options.label == "Gym");
    CHECK(add.options.snooze_minutes == 5);
    CHECK(add.options.fade_in_seconds == 60);
    CHECK(add.options.sound == "bell.wav");
}

TEST_CASE("cli::parse: other commands") {
    CHECK(std::get<cli::Remove>(cli::parse({"rm", "3"}).command).id == 3);
    CHECK(std::get<cli::SetEnabled>(cli::parse({"disable", "2"}).command).enabled == false);
    CHECK(std::get<cli::SetEnabled>(cli::parse({"enable", "2"}).command).enabled == true);
    CHECK(std::get<cli::SetHourFormat>(cli::parse({"set", "hour-format", "12"}).command).format == HourFormat::H12);
    const auto edit = std::get<cli::Edit>(cli::parse({"edit", "7", "--time", "19:00"}).command);
    CHECK(edit.id == 7);
    CHECK(edit.options.time == TimeOfDay{19, 0});
    CHECK_FALSE(edit.options.label.has_value());
    CHECK(std::holds_alternative<cli::List>(cli::parse({"list"}).command));
    CHECK(std::get<cli::Run>(cli::parse({"run", "--plain"}).command).plain);
    CHECK_FALSE(std::get<cli::Run>(cli::parse({"run"}).command).plain);
    CHECK_THROWS_AS(cli::parse({"run", "--fancy"}), cli::UsageError);
    CHECK(std::holds_alternative<cli::Help>(cli::parse({"--help"}).command));
}

TEST_CASE("cli::parse: helpful errors") {
    CHECK_THROWS_WITH(cli::parse({"add"}), ContainsSubstring("missing alarm time"));
    CHECK_THROWS_WITH(cli::parse({"add", "25:00"}), ContainsSubstring("not a valid time"));
    CHECK_THROWS_WITH(cli::parse({"add", "7:00", "--days", "funday"}), ContainsSubstring("invalid --days"));
    CHECK_THROWS_WITH(cli::parse({"add", "7:00", "--snooze", "0"}), ContainsSubstring("--snooze"));
    CHECK_THROWS_WITH(cli::parse({"add", "7:00", "--bogus"}), ContainsSubstring("unknown option"));
    CHECK_THROWS_WITH(cli::parse({"remove", "abc"}), ContainsSubstring("alarm id"));
    CHECK_THROWS_WITH(cli::parse({"list", "extra"}), ContainsSubstring("unexpected"));
    CHECK_THROWS_WITH(cli::parse({"frobnicate"}), ContainsSubstring("unknown command"));
    CHECK_THROWS_WITH(cli::parse({"--data"}), ContainsSubstring("--data"));
}

TEST_CASE("run_command edits the alarm file") {
    TempDir dir;
    StateFile file(dir.path() / "alarms.json");
    FakeClock clock(utc(2026, 1, 10, 6, 0));
    const TestZone tz{minutes{0}};
    std::ostringstream out, err;

    auto run = [&](std::vector<std::string> args) {
        out.str("");
        err.str("");
        return run_command(cli::parse(args).command, file, clock, tz, out, err);
    };

    REQUIRE(run({"add", "7:30", "--label", "Work", "--days", "weekdays"}) == 0);
    CHECK_THAT(out.str(), ContainsSubstring("Added alarm 1"));
    REQUIRE(run({"add", "9:00"}) == 0);
    CHECK_THAT(out.str(), ContainsSubstring("Added alarm 2"));

    REQUIRE(run({"list"}) == 0);
    CHECK_THAT(out.str(), ContainsSubstring("Work"));
    CHECK_THAT(out.str(), ContainsSubstring("Weekdays"));

    REQUIRE(run({"edit", "2", "--time", "8:15", "--label", "Late"}) == 0);
    REQUIRE(run({"disable", "1"}) == 0);
    REQUIRE(run({"remove", "1"}) == 0);
    CHECK(run({"remove", "1"}) == 1);
    CHECK_THAT(err.str(), ContainsSubstring("No alarm with id 1"));

    const auto state = StateFile(file.path()).load().state;
    REQUIRE(state.alarms.size() == 1);
    CHECK(state.alarms[0].id == 2);
    CHECK(state.alarms[0].time == TimeOfDay{8, 15});
    CHECK(state.alarms[0].label == "Late");
    CHECK(state.alarms[0].updated_at == clock.now());
}
