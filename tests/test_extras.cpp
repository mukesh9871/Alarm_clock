// Phase 4 features: challenges, sleep cycles, durations.
#include "core/challenge.hpp"
#include "core/duration.hpp"
#include "core/sleep_cycles.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace ac;
using namespace std::chrono;

TEST_CASE("make_math_problem is deterministic and in range") {
    CHECK(make_math_problem(42).question == make_math_problem(42).question);
    std::set<std::string> distinct;
    for (std::uint32_t seed = 0; seed < 500; ++seed) {
        const auto p = make_math_problem(seed);
        distinct.insert(p.question);
        int a = 0, b = 0, c = 0;
        REQUIRE(std::sscanf(p.question.c_str(), "%d x %d + %d = ?", &a, &b, &c) == 3);
        CHECK((a >= 3 && a <= 12));
        CHECK((b >= 3 && b <= 12));
        CHECK((c >= 10 && c <= 99));
        CHECK(p.answer == a * b + c);
    }
    CHECK(distinct.size() > 300);  // varied enough that you can't memorise answers
}

TEST_CASE("parse_challenge") {
    CHECK(parse_challenge("math") == ChallengeKind::Math);
    CHECK(parse_challenge("none") == ChallengeKind::None);
    CHECK(parse_challenge("") == ChallengeKind::None);
    CHECK_FALSE(parse_challenge("chess").has_value());
    CHECK(challenge_to_string(ChallengeKind::Math) == "math");
}

TEST_CASE("bedtimes_for counts back whole sleep cycles plus time to fall asleep") {
    const auto opts = bedtimes_for({7, 0});
    REQUIRE(opts.size() == 4);
    CHECK(opts[0].cycles == 6);
    CHECK(opts[0].time == TimeOfDay{21, 45});  // 07:00 - 9h - 15m, across midnight
    CHECK(opts[0].sleep == hours{9});
    CHECK(opts[1].time == TimeOfDay{23, 15});
    CHECK(opts[2].time == TimeOfDay{0, 45});
    CHECK(opts[3].time == TimeOfDay{2, 15});
    CHECK(opts[3].cycles == 3);
}

TEST_CASE("wake_times_for counts forward") {
    const auto opts = wake_times_for({23, 0}, minutes{0});
    REQUIRE(opts.size() == 4);
    CHECK(opts[0].time == TimeOfDay{3, 30});
    CHECK(opts[3].time == TimeOfDay{8, 0});
}

TEST_CASE("parse_duration accepts common forms") {
    CHECK(parse_duration("10") == minutes{10});
    CHECK(parse_duration("90s") == seconds{90});
    CHECK(parse_duration("10m") == minutes{10});
    CHECK(parse_duration("1h30m") == minutes{90});
    CHECK(parse_duration("1h 5m 10s") == hours{1} + minutes{5} + seconds{10});
    CHECK(parse_duration("2H") == hours{2});
    CHECK(parse_duration("25:00") == minutes{25});
    CHECK(parse_duration("1:30:00") == minutes{90});
    CHECK(parse_duration("24h") == hours{24});
}

TEST_CASE("parse_duration rejects nonsense") {
    for (const char* bad : {"", "0", "0s", "abc", "10x", "1h30", "30m1h", "5m5m", "25h", "1:5", "1:60", ":30",
                            "1::00", "m", "-5m", "1234567m"}) {
        INFO(bad);
        CHECK_FALSE(parse_duration(bad).has_value());
    }
}

TEST_CASE("format_hms") {
    CHECK(format_hms(seconds{0}) == "00:00");
    CHECK(format_hms(seconds{309}) == "05:09");
    CHECK(format_hms(seconds{3909}) == "1:05:09");
    CHECK(format_hms(seconds{-5}) == "00:00");
    CHECK(format_hms_tenths(milliseconds{3909'340}) == "1:05:09.3");
}
