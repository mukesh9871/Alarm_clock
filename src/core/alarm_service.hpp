#pragma once

#include "core/alarm.hpp"
#include "core/time_zone.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace ac {

struct AlarmEvent {
    enum class Kind {
        Ring,    // alarm is due now (or only slightly late) - start ringing
        Missed,  // alarm was due long ago (e.g. PC was asleep) - report, don't ring
    };
    Kind kind;
    int alarm_id;
    TimePoint scheduled;
    std::chrono::seconds late;
};

// Owns the alarms and decides, on each tick, which ones fire.
// Pure logic: no threads, no I/O, no real clock - drive it with tick(now).
class AlarmService {
public:
    explicit AlarmService(const TimeZone& tz, std::chrono::minutes missed_grace = std::chrono::minutes{30});

    // Adds (or, if the id exists, replaces) an alarm and arms it relative to `now`. Returns its id.
    int upsert(Alarm alarm, TimePoint now);
    bool remove(int id);
    bool set_enabled(int id, bool enabled, TimePoint now);

    [[nodiscard]] const Alarm* find(int id) const;
    [[nodiscard]] std::vector<Alarm> alarms() const;
    [[nodiscard]] std::optional<TimePoint> next_fire(int id) const;
    // The soonest armed alarm, if any.
    [[nodiscard]] std::optional<std::pair<int, TimePoint>> next_due() const;
    [[nodiscard]] std::vector<int> ringing() const;

    // Advances time. Fires every alarm whose scheduled time is <= now.
    std::vector<AlarmEvent> tick(TimePoint now);

    // Stops a ringing alarm and re-arms (or disables, for one-shot alarms).
    void dismiss(int id, TimePoint now);

private:
    struct Entry {
        Alarm alarm;
        std::optional<TimePoint> next;
        bool ringing = false;
    };

    Entry* entry(int id);
    [[nodiscard]] const Entry* entry(int id) const;
    void arm(Entry& e, TimePoint now);
    void finish(Entry& e, TimePoint now);

    const TimeZone& tz_;
    std::chrono::minutes missed_grace_;
    std::vector<Entry> entries_;
    int next_id_ = 1;
};

}  // namespace ac
