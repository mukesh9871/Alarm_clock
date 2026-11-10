#include "core/saved_state.hpp"
#include "platform/state_file.hpp"
#include "support/fakes.hpp"
#include "support/test_zone.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>

using namespace ac;
using namespace ac::test;
using Catch::Matchers::ContainsSubstring;

namespace {
SavedState sample() {
    SavedState s;
    s.settings.hour_format = HourFormat::H12;
    s.settings.missed_grace_minutes = 15;
    s.alarms.push_back(Alarm{.id = 1, .time = {6, 45}, .label = "Gym \"early\" ☀", .days = DaySet::weekdays()});
    s.alarms.push_back(Alarm{.id = 4,
                             .time = {22, 0},
                             .enabled = false,
                             .snooze_minutes = 5,
                             .sound = "C:/sounds/bell.mp3",
                             .fade_in_seconds = 0,
                             .challenge = ChallengeKind::Math,
                             .updated_at = utc(2026, 5, 1, 12)});
    s.last_seen = utc(2026, 11, 8, 6, 0);
    s.pending.push_back({1, utc(2026, 11, 8, 6, 54)});
    return s;
}
}  // namespace

TEST_CASE("SavedState JSON round trip") {
    const auto s = sample();
    CHECK(state_from_json(to_json(s)) == s);
}

TEST_CASE("state_from_json fills defaults for optional fields") {
    const auto s = state_from_json(R"({"version":1,"alarms":[{"id":3,"time":"07:00"}]})");
    REQUIRE(s.alarms.size() == 1);
    const Alarm& a = s.alarms[0];
    CHECK(a.time == TimeOfDay{7, 0});
    CHECK(a.enabled);
    CHECK(a.days.empty());
    CHECK(a.snooze_minutes == 9);
    CHECK(s.settings == Settings{});
    CHECK_FALSE(s.last_seen.has_value());
}

TEST_CASE("state_from_json rejects malformed input with a reason") {
    using Catch::Matchers::ContainsSubstring;
    CHECK_THROWS_AS(state_from_json("not json"), StateFormatError);
    CHECK_THROWS_WITH(state_from_json(R"({"version":99})"), ContainsSubstring("version"));
    CHECK_THROWS_WITH(state_from_json(R"({"version":1,"alarms":[{"id":1,"time":"25:00"}]})"),
                      ContainsSubstring("invalid time"));
    CHECK_THROWS_WITH(state_from_json(R"({"version":1,"alarms":[{"id":1,"time":"07:00","days":"funday"}]})"),
                      ContainsSubstring("invalid days"));
    CHECK_THROWS_WITH(
        state_from_json(R"({"version":1,"alarms":[{"id":1,"time":"07:00"},{"id":1,"time":"08:00"}]})"),
        ContainsSubstring("duplicate"));
    CHECK_THROWS_AS(state_from_json(R"({"version":1,"alarms":[{"time":"07:00"}]})"), StateFormatError);
    CHECK_THROWS_WITH(state_from_json(R"({"version":1,"alarms":[{"id":1,"time":"07:00","challenge":"chess"}]})"),
                      ContainsSubstring("invalid challenge"));
}

TEST_CASE("state_from_json drops pending rings for alarms that no longer exist") {
    const auto s = state_from_json(
        R"({"version":1,"alarms":[{"id":1,"time":"07:00"}],"pending":[{"id":1,"due_unix":60},{"id":2,"due_unix":60}]})");
    REQUIRE(s.pending.size() == 1);
    CHECK(s.pending[0].alarm_id == 1);
    CHECK(s.pending[0].due == TimePoint{std::chrono::seconds{60}});
}

TEST_CASE("StateFile: missing file loads empty, save then load round-trips") {
    TempDir dir;
    StateFile file(dir.path() / "nested" / "alarms.json");
    auto first = file.load();
    CHECK(first.state == SavedState{});
    CHECK_FALSE(first.warning.has_value());

    file.save(sample());
    CHECK_FALSE(file.changed_on_disk());
    StateFile other(file.path());
    CHECK(other.load().state == sample());
}

TEST_CASE("StateFile: corrupt file is backed up, not overwritten") {
    TempDir dir;
    const auto path = dir.path() / "alarms.json";
    {
        std::ofstream(path) << "{ this is not json";
    }
    StateFile file(path);
    const auto result = file.load();
    CHECK(result.state == SavedState{});
    REQUIRE(result.warning.has_value());
    CHECK(std::filesystem::exists(dir.path() / "alarms.json.corrupt-1"));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("StateFile: detects external modification") {
    TempDir dir;
    StateFile file(dir.path() / "alarms.json");
    file.save(sample());
    CHECK_FALSE(file.changed_on_disk());

    StateFile writer(file.path());
    auto s = sample();
    s.alarms.pop_back();
    writer.save(s);
    // Force a distinct timestamp in case the filesystem's resolution is coarse.
    std::filesystem::last_write_time(file.path(), std::filesystem::file_time_type::clock::now() + std::chrono::seconds{5});
    CHECK(file.changed_on_disk());
}

TEST_CASE("StateFile: save leaves no temp files behind") {
    TempDir dir;
    StateFile file(dir.path() / "alarms.json");
    file.save(sample());
    file.save(SavedState{});
    int entries = 0;
    for ([[maybe_unused]] const auto& e : std::filesystem::directory_iterator(dir.path())) ++entries;
    CHECK(entries == 1);
}

TEST_CASE("StateFile: OnCorrupt::Leave keeps an unreadable file and refuses to overwrite it") {
    TempDir dir;
    const auto path = dir.path() / "alarms.json";
    {
        std::ofstream(path) << "{ half-written";
    }
    StateFile file(path);
    const auto result = file.load(StateFile::OnCorrupt::Leave);
    CHECK(result.unreadable);
    REQUIRE(result.warning.has_value());
    CHECK_THAT(*result.warning, ContainsSubstring("left as is"));
    CHECK(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(dir.path() / "alarms.json.corrupt-1"));
    CHECK_FALSE(file.changed_on_disk());  // no reload loop until it changes again
    CHECK_THROWS_WITH(file.save(sample()), ContainsSubstring("not overwriting"));

    // Once someone fixes the file, it loads and saves normally again.
    StateFile(path).save(sample());
    bump_mtime(path);
    CHECK(file.changed_on_disk());
    CHECK(file.load(StateFile::OnCorrupt::Leave).state == sample());
    CHECK_NOTHROW(file.save(SavedState{}));
}

#ifdef _WIN32
TEST_CASE("StateFile: a corrupt file that can't be moved is copied to the backup instead") {
    TempDir dir;
    const auto path = dir.path() / "alarms.json";
    {
        std::ofstream(path) << "{ this is not json";
    }
    // On Windows an open file can't be renamed, as when an editor or virus scanner holds it.
    std::ifstream held(path);
    REQUIRE(held.is_open());
    StateFile file(path);
    const auto result = file.load();
    CHECK(result.unreadable);
    REQUIRE(result.warning.has_value());
    CHECK_THAT(*result.warning, ContainsSubstring("copied to"));
    CHECK(std::filesystem::exists(dir.path() / "alarms.json.corrupt-1"));
}
#endif
