#include "core/alarm_service.hpp"

#include "core/schedule.hpp"

#include <algorithm>
#include <tuple>

namespace ac {

AlarmService::AlarmService(const TimeZone& tz, ServiceConfig config) : tz_(tz), config_(config) {}

int AlarmService::upsert(Alarm alarm, TimePoint arm_from) {
    if (alarm.id <= 0) alarm.id = next_alarm_id(alarms());

    Entry* e = entry(alarm.id);
    if (e == nullptr) {
        entries_.push_back(Entry{});
        e = &entries_.back();
    }
    *e = Entry{.alarm = std::move(alarm)};
    arm(*e, arm_from);
    return e->alarm.id;
}

bool AlarmService::remove(int id) {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [id](const Entry& e) { return e.alarm.id == id; });
    if (it == entries_.end()) return false;
    entries_.erase(it);
    return true;
}

bool AlarmService::set_enabled(int id, bool enabled, TimePoint now) {
    Entry* e = entry(id);
    if (e == nullptr) return false;
    Alarm a = e->alarm;
    a.enabled = enabled;
    upsert(std::move(a), now);
    return true;
}

void AlarmService::sync(const std::vector<Alarm>& wanted, TimePoint now) {
    std::erase_if(entries_, [&](const Entry& e) {
        return std::none_of(wanted.begin(), wanted.end(), [&](const Alarm& a) { return a.id == e.alarm.id; });
    });
    for (const Alarm& a : wanted) {
        const Entry* existing = entry(a.id);
        if (existing == nullptr || !(existing->alarm == a)) upsert(a, now);
    }
}

const Alarm* AlarmService::find(int id) const {
    const Entry* e = entry(id);
    return e ? &e->alarm : nullptr;
}

std::vector<Alarm> AlarmService::alarms() const {
    std::vector<Alarm> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.alarm);
    std::sort(out.begin(), out.end(), [](const Alarm& a, const Alarm& b) {
        return std::tie(a.time, a.id) < std::tie(b.time, b.id);
    });
    return out;
}

std::optional<TimePoint> AlarmService::next_fire(int id) const {
    const Entry* e = entry(id);
    return e ? e->next : std::nullopt;
}

std::optional<std::pair<int, TimePoint>> AlarmService::next_due() const {
    std::optional<std::pair<int, TimePoint>> best;
    for (const auto& e : entries_) {
        if (e.next && (!best || *e.next < best->second)) best = std::pair{e.alarm.id, *e.next};
    }
    return best;
}

std::vector<int> AlarmService::ringing() const {
    std::vector<int> out;
    for (const auto& e : entries_) {
        if (e.ring_started) out.push_back(e.alarm.id);
    }
    return out;
}

bool AlarmService::is_snoozed(int id) const {
    const Entry* e = entry(id);
    return e && e->snoozed;
}

std::vector<PendingRing> AlarmService::pending() const {
    std::vector<PendingRing> out;
    for (const auto& e : entries_) {
        if (e.ring_started) {
            out.push_back({e.alarm.id, *e.ring_started});
        } else if (e.snoozed && e.next) {
            out.push_back({e.alarm.id, *e.next});
        }
    }
    std::sort(out.begin(), out.end(), [](const PendingRing& a, const PendingRing& b) { return a.alarm_id < b.alarm_id; });
    return out;
}

void AlarmService::restore_pending(const PendingRing& p) {
    Entry* e = entry(p.alarm_id);
    if (e == nullptr || !e->alarm.enabled) return;
    e->ring_started.reset();
    e->snoozed = true;
    e->next = p.due;
}

std::vector<AlarmEvent> AlarmService::tick(TimePoint now) {
    std::vector<AlarmEvent> events;
    for (auto& e : entries_) {
        if (e.ring_started) {
            if (now - *e.ring_started < config_.ring_timeout) continue;
            if (e.auto_snoozes < config_.max_auto_snoozes) {
                events.push_back({AlarmEvent::Kind::AutoSnoozed, e.alarm.id, *e.ring_started});
                ++e.auto_snoozes;
                snooze_entry(e, now);
            } else {
                events.push_back({AlarmEvent::Kind::TimedOut, e.alarm.id, *e.ring_started});
                finish(e, now);
            }
            continue;
        }

        if (!e.next || *e.next > now) continue;

        const TimePoint scheduled = *e.next;
        const auto late = now - scheduled;
        if (late <= config_.missed_grace) {
            events.push_back({AlarmEvent::Kind::Ring, e.alarm.id, scheduled, late});
            e.ring_started = now;
            e.snoozed = false;
            e.next.reset();  // re-armed on dismiss/snooze
        } else {
            events.push_back({AlarmEvent::Kind::Missed, e.alarm.id, scheduled, late});
            finish(e, now);
        }
    }
    return events;
}

void AlarmService::dismiss(int id, TimePoint now) {
    Entry* e = entry(id);
    if (e == nullptr || !(e->ring_started || e->snoozed)) return;
    finish(*e, now);
}

void AlarmService::snooze(int id, TimePoint now) {
    Entry* e = entry(id);
    if (e == nullptr || !e->ring_started) return;
    e->auto_snoozes = 0;  // a human responded
    snooze_entry(*e, now);
}

AlarmService::Entry* AlarmService::entry(int id) {
    for (auto& e : entries_) {
        if (e.alarm.id == id) return &e;
    }
    return nullptr;
}

const AlarmService::Entry* AlarmService::entry(int id) const {
    return const_cast<AlarmService*>(this)->entry(id);
}

void AlarmService::arm(Entry& e, TimePoint from) {
    e.next = e.alarm.enabled ? std::optional{next_occurrence(e.alarm.time, from, tz_, e.alarm.days)} : std::nullopt;
}

void AlarmService::finish(Entry& e, TimePoint now) {
    e.ring_started.reset();
    e.snoozed = false;
    e.auto_snoozes = 0;
    if (!e.alarm.repeats()) e.alarm.enabled = false;  // one-shot: consumed
    arm(e, now);
}

void AlarmService::snooze_entry(Entry& e, TimePoint now) {
    e.ring_started.reset();
    e.snoozed = true;
    e.next = now + std::chrono::minutes{std::max(1, e.alarm.snooze_minutes)};
}

}  // namespace ac
