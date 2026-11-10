#pragma once

#include "core/alarm.hpp"
#include "core/time_zone.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace ac {

struct AlarmEvent {
    enum class Kind {
        Ring,         // alarm is due now (or only slightly late) - start ringing
        Missed,       // alarm was due long ago (e.g. PC asleep or app closed) - report, don't ring
        AutoSnoozed,  // rang unanswered for ring_timeout - snoozed automatically
        TimedOut,     // auto-snoozed too many times - gave up
    };
    Kind kind;
    int alarm_id;
    TimePoint scheduled;
    std::chrono::seconds late{0};
};

struct ServiceConfig {
    std::chrono::minutes missed_grace{30};  // later than this = Missed instead of Ring
    std::chrono::minutes ring_timeout{10};  // unanswered ringing auto-snoozes after this
    int max_auto_snoozes = 3;               // then the alarm stops for this occurrence
};

// Owns the alarms and decides, on each tick, which ones fire.
// Pure logic: no threads, no I/O, no real clock - drive it with tick(now).
class AlarmService {
public:
    explicit AlarmService(const TimeZone& tz, ServiceConfig config = {});

    // Adds (or, if the id exists, replaces) an alarm and arms it: it will fire at its
    // first occurrence strictly after `arm_from`. Returns its id.
    int upsert(Alarm alarm, TimePoint arm_from);
    bool remove(int id);
    bool set_enabled(int id, bool enabled, TimePoint now);

    // Makes the alarm list match `wanted` (e.g. after the file was edited elsewhere).
    // Unchanged alarms keep their runtime state (ringing, snoozed); others are (re)armed.
    void sync(const std::vector<Alarm>& wanted, TimePoint now);

    [[nodiscard]] const Alarm* find(int id) const;
    [[nodiscard]] std::vector<Alarm> alarms() const;
    [[nodiscard]] std::optional<TimePoint> next_fire(int id) const;
    [[nodiscard]] std::optional<std::pair<int, TimePoint>> next_due() const;  // soonest armed alarm
    [[nodiscard]] std::vector<int> ringing() const;
    [[nodiscard]] bool is_snoozed(int id) const;

    // Snoozed or ringing occurrences, to save across restarts. A ringing alarm is due
    // when it started ringing.
    [[nodiscard]] std::vector<PendingRing> pending() const;
    // Resumes a saved occurrence: the (enabled) alarm fires at `due` as a snooze would.
    // Ignored for unknown or disabled alarms.
    void restore_pending(const PendingRing& p);

    // Advances time. Fires every alarm whose scheduled time is <= now.
    std::vector<AlarmEvent> tick(TimePoint now);

    // Stops a ringing alarm and re-arms it (repeating) or disables it (one-shot).
    void dismiss(int id, TimePoint now);
    // Stops a ringing alarm and rings again after its snooze interval.
    void snooze(int id, TimePoint now);

    [[nodiscard]] const ServiceConfig& config() const { return config_; }
    void set_config(ServiceConfig config) { config_ = config; }

private:
    struct Entry {
        Alarm alarm;
        std::optional<TimePoint> next{};
        std::optional<TimePoint> ring_started{};  // set while ringing
        bool snoozed = false;
        int auto_snoozes = 0;
    };

    Entry* entry(int id);
    [[nodiscard]] const Entry* entry(int id) const;
    void arm(Entry& e, TimePoint from);
    void finish(Entry& e, TimePoint now);
    void snooze_entry(Entry& e, TimePoint now);

    const TimeZone& tz_;
    ServiceConfig config_;
    std::vector<Entry> entries_;
    int next_id_ = 1;
};

}  // namespace ac
