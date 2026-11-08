#include "core/saved_state.hpp"

#include "core/days.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>

namespace ac {

namespace {

using nlohmann::json;
constexpr int kVersion = 1;

json alarm_to_json(const Alarm& a) {
    return json{
        {"id", a.id},
        {"time", format_time_of_day(a.time, HourFormat::H24)},
        {"label", a.label},
        {"enabled", a.enabled},
        {"days", days_to_string(a.days)},
        {"snooze_minutes", a.snooze_minutes},
        {"sound", a.sound},
        {"fade_in_seconds", a.fade_in_seconds},
        {"updated_unix", a.updated_at.time_since_epoch().count()},
    };
}

Alarm alarm_from_json(const json& j) {
    Alarm a;
    a.id = j.at("id").get<int>();
    if (a.id <= 0) throw StateFormatError("alarm id must be positive");

    const auto time_text = j.at("time").get<std::string>();
    const auto time = parse_time_of_day(time_text);
    if (!time) throw StateFormatError("alarm " + std::to_string(a.id) + ": invalid time '" + time_text + "'");
    a.time = *time;

    const auto days_text = j.value("days", std::string{});
    const auto days = parse_days(days_text);
    if (!days) throw StateFormatError("alarm " + std::to_string(a.id) + ": invalid days '" + days_text + "'");
    a.days = *days;

    a.label = j.value("label", std::string{});
    a.enabled = j.value("enabled", true);
    a.snooze_minutes = std::clamp(j.value("snooze_minutes", 9), 1, 60);
    a.sound = j.value("sound", std::string{});
    a.fade_in_seconds = std::clamp(j.value("fade_in_seconds", 30), 0, 600);
    a.updated_at = TimePoint{std::chrono::seconds{j.value("updated_unix", std::int64_t{0})}};
    return a;
}

}  // namespace

std::string to_json(const SavedState& state) {
    json alarms = json::array();
    for (const auto& a : state.alarms) alarms.push_back(alarm_to_json(a));

    json j{
        {"version", kVersion},
        {"settings",
         {
             {"hour_format", state.settings.hour_format == HourFormat::H12 ? "12h" : "24h"},
             {"missed_grace_minutes", state.settings.missed_grace_minutes},
             {"ring_timeout_minutes", state.settings.ring_timeout_minutes},
         }},
        {"alarms", alarms},
    };
    if (state.last_seen) j["last_seen_unix"] = state.last_seen->time_since_epoch().count();
    return j.dump(2) + "\n";
}

SavedState state_from_json(std::string_view text) {
    try {
        const json j = json::parse(text);
        if (!j.is_object()) throw StateFormatError("top level must be an object");
        const int version = j.value("version", 0);
        if (version != kVersion) throw StateFormatError("unsupported version " + std::to_string(version));

        SavedState state;
        if (const auto it = j.find("settings"); it != j.end()) {
            const auto fmt = it->value("hour_format", std::string{"24h"});
            if (fmt != "12h" && fmt != "24h") throw StateFormatError("hour_format must be \"12h\" or \"24h\"");
            state.settings.hour_format = fmt == "12h" ? HourFormat::H12 : HourFormat::H24;
            state.settings.missed_grace_minutes = std::clamp(it->value("missed_grace_minutes", 30), 1, 24 * 60);
            state.settings.ring_timeout_minutes = std::clamp(it->value("ring_timeout_minutes", 10), 1, 120);
        }

        std::set<int> ids;
        for (const auto& item : j.value("alarms", json::array())) {
            Alarm a = alarm_from_json(item);
            if (!ids.insert(a.id).second) throw StateFormatError("duplicate alarm id " + std::to_string(a.id));
            state.alarms.push_back(std::move(a));
        }

        if (const auto it = j.find("last_seen_unix"); it != j.end()) {
            state.last_seen = TimePoint{std::chrono::seconds{it->get<std::int64_t>()}};
        }
        return state;
    } catch (const json::exception& e) {
        throw StateFormatError(e.what());
    }
}

}  // namespace ac
