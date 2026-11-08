#include "app/session.hpp"

#include "core/time_format.hpp"

#include <algorithm>
#include <exception>

namespace ac {

using namespace std::chrono_literals;

namespace {
constexpr auto kLastSeenInterval = 60s;  // how often "still running" is recorded
}

AlarmSession::AlarmSession(StateFile& file, const Clock& clock, const TimeZone& tz, AudioPlayer& audio,
                           SoundResolver resolve_sound)
    : file_(file), clock_(clock), tz_(tz), audio_(audio), resolve_sound_(std::move(resolve_sound)), service_(tz) {}

std::vector<Notice> AlarmSession::start() {
    std::vector<Notice> notices;
    auto loaded = file_.load();
    if (loaded.warning) notices.push_back({Notice::Level::Warning, *loaded.warning});

    settings_ = loaded.state.settings;
    apply_settings();

    const TimePoint now = clock_.now();
    const TimePoint last_seen = loaded.state.last_seen.value_or(now);
    for (const Alarm& a : loaded.state.alarms) {
        // Arm from when we last watched the clock, so anything due since then is caught,
        // but never from before the alarm existed.
        const TimePoint from = std::min(now, std::max(last_seen, a.updated_at));
        service_.upsert(a, from);
    }

    auto events = to_notices(service_.tick(now), /*at_startup=*/true);
    notices.insert(notices.end(), events.begin(), events.end());
    persist();
    return notices;
}

std::vector<Notice> AlarmSession::tick() {
    std::vector<Notice> notices;
    const TimePoint now = clock_.now();

    if (file_.changed_on_disk()) {
        auto loaded = file_.load();
        if (loaded.warning) notices.push_back({Notice::Level::Warning, *loaded.warning});
        settings_ = loaded.state.settings;
        apply_settings();
        service_.sync(loaded.state.alarms, now);
        last_saved_ = loaded.state;
        last_saved_.alarms = service_.alarms();
        notices.push_back({Notice::Level::Info, "Alarm list reloaded (changed outside this window)."});
    }

    auto events = to_notices(service_.tick(now), /*at_startup=*/false);
    notices.insert(notices.end(), events.begin(), events.end());
    stop_audio_if_quiet();
    persist_if_changed();
    if (save_error_ && *save_error_ != reported_save_error_) {
        notices.push_back({Notice::Level::Warning, *save_error_});
        reported_save_error_ = *save_error_;
    }
    return notices;
}

void AlarmSession::shutdown() {
    audio_.stop();
    persist();
}

void AlarmSession::snooze() {
    const TimePoint now = clock_.now();
    for (int id : service_.ringing()) service_.snooze(id, now);
    stop_audio_if_quiet();
}

void AlarmSession::dismiss() {
    const TimePoint now = clock_.now();
    for (int id : service_.ringing()) service_.dismiss(id, now);
    stop_audio_if_quiet();
    persist_if_changed();  // one-shot alarms become disabled
}

int AlarmSession::save_alarm(Alarm alarm) {
    const TimePoint now = clock_.now();
    if (alarm.id <= 0) {
        int max_id = 0;
        for (const auto& a : service_.alarms()) max_id = std::max(max_id, a.id);
        alarm.id = max_id + 1;
    }
    alarm.updated_at = now;
    const int id = service_.upsert(std::move(alarm), now);
    stop_audio_if_quiet();
    persist();
    return id;
}

bool AlarmSession::remove_alarm(int id) {
    const bool ok = service_.remove(id);
    stop_audio_if_quiet();
    persist();
    return ok;
}

bool AlarmSession::toggle_alarm(int id) {
    const Alarm* a = service_.find(id);
    if (a == nullptr) return false;
    Alarm copy = *a;
    copy.enabled = !copy.enabled;
    save_alarm(std::move(copy));
    return true;
}

void AlarmSession::set_hour_format(HourFormat fmt) {
    settings_.hour_format = fmt;
    persist();
}

std::string AlarmSession::describe(int alarm_id) const {
    const Alarm* a = service_.find(alarm_id);
    if (a == nullptr) return "alarm #" + std::to_string(alarm_id);
    std::string s = format_time_of_day(a->time, settings_.hour_format);
    if (!a->label.empty()) s += " \"" + a->label + "\"";
    return s;
}

std::vector<Notice> AlarmSession::to_notices(const std::vector<AlarmEvent>& events, bool at_startup) {
    std::vector<Notice> out;
    for (const auto& ev : events) {
        const std::string name = describe(ev.alarm_id);
        switch (ev.kind) {
            case AlarmEvent::Kind::Ring: {
                const Alarm* a = service_.find(ev.alarm_id);
                const bool played = audio_.start(resolve_sound_(a ? a->sound : std::string{}),
                                                 std::chrono::seconds{a ? a->fade_in_seconds : 0});
                out.push_back({Notice::Level::Alarm, "ALARM " + name});
                if (!played) out.push_back({Notice::Level::Warning, "Could not play the alarm sound - beeping instead."});
                break;
            }
            case AlarmEvent::Kind::Missed:
                out.push_back({Notice::Level::Warning,
                               "Missed alarm " + name + " (" + format_duration(ev.late) + " ago" +
                                   (at_startup ? ", while the clock wasn't running)." : ", computer was asleep?).")});
                break;
            case AlarmEvent::Kind::AutoSnoozed:
                out.push_back({Notice::Level::Info, "No response - snoozed " + name + " automatically."});
                break;
            case AlarmEvent::Kind::TimedOut:
                out.push_back({Notice::Level::Warning, "Alarm " + name + " rang unanswered and was stopped."});
                break;
        }
    }
    return out;
}

void AlarmSession::apply_settings() {
    service_.set_config(ServiceConfig{
        .missed_grace = std::chrono::minutes{settings_.missed_grace_minutes},
        .ring_timeout = std::chrono::minutes{settings_.ring_timeout_minutes},
    });
}

void AlarmSession::persist() {
    SavedState state{.settings = settings_, .alarms = service_.alarms(), .last_seen = clock_.now()};
    try {
        file_.save(state);
        save_error_.reset();
    } catch (const std::exception& e) {
        // Keep running - ringing alarms matters more than saving. Retry on the next change.
        save_error_ = std::string("Could not save alarms: ") + e.what();
    }
    last_saved_ = std::move(state);
}

void AlarmSession::persist_if_changed() {
    const bool stale = !last_saved_.last_seen || clock_.now() - *last_saved_.last_seen >= kLastSeenInterval;
    if (stale || last_saved_.settings != settings_ || last_saved_.alarms != service_.alarms()) persist();
}

void AlarmSession::stop_audio_if_quiet() {
    if (service_.ringing().empty() && audio_.is_playing()) audio_.stop();
}

}  // namespace ac
