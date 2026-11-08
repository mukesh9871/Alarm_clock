#include "core/alarm_service.hpp"

#include "core/schedule.hpp"

#include <algorithm>

namespace ac {

AlarmService::AlarmService(const TimeZone& tz, std::chrono::minutes missed_grace)
    : tz_(tz), missed_grace_(missed_grace) {}

int AlarmService::upsert(Alarm alarm, TimePoint now) {
    if (alarm.id == 0) alarm.id = next_id_;
    next_id_ = std::max(next_id_, alarm.id + 1);

    Entry* e = entry(alarm.id);
    if (e == nullptr) {
        entries_.push_back(Entry{});
        e = &entries_.back();
    }
    e->alarm = std::move(alarm);
    e->ringing = false;
    arm(*e, now);
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
    e->alarm.enabled = enabled;
    e->ringing = false;
    arm(*e, now);
    return true;
}

const Alarm* AlarmService::find(int id) const {
    const Entry* e = entry(id);
    return e ? &e->alarm : nullptr;
}

std::vector<Alarm> AlarmService::alarms() const {
    std::vector<Alarm> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.alarm);
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
        if (e.ringing) out.push_back(e.alarm.id);
    }
    return out;
}

std::vector<AlarmEvent> AlarmService::tick(TimePoint now) {
    std::vector<AlarmEvent> events;
    for (auto& e : entries_) {
        if (!e.next || *e.next > now) continue;

        const TimePoint scheduled = *e.next;
        const auto late = now - scheduled;
        if (late <= missed_grace_) {
            events.push_back({AlarmEvent::Kind::Ring, e.alarm.id, scheduled, late});
            e.ringing = true;
            e.next.reset();  // re-armed on dismiss
        } else {
            events.push_back({AlarmEvent::Kind::Missed, e.alarm.id, scheduled, late});
            finish(e, now);
        }
    }
    return events;
}

void AlarmService::dismiss(int id, TimePoint now) {
    Entry* e = entry(id);
    if (e == nullptr || !e->ringing) return;
    e->ringing = false;
    finish(*e, now);
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

void AlarmService::arm(Entry& e, TimePoint now) {
    if (!e.alarm.enabled) {
        e.next.reset();
        return;
    }
    e.next = next_occurrence(e.alarm.time, now, tz_);
}

void AlarmService::finish(Entry& e, TimePoint /*now*/) {
    // One-shot alarm: done after it fires (or is missed).
    e.alarm.enabled = false;
    e.next.reset();
}

}  // namespace ac
