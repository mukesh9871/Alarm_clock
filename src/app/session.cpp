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
                           SoundResolver resolve_sound, PowerManager* power)
    : file_(file),
      clock_(clock),
      tz_(tz),
      audio_(audio),
      resolve_sound_(std::move(resolve_sound)),
      power_(power),
      service_(tz) {}

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
    for (const PendingRing& p : loaded.state.pending) service_.restore_pending(p);

    auto events = to_notices(service_.tick(now), /*at_startup=*/true);
    notices.insert(notices.end(), events.begin(), events.end());
    update_power();
    persist();
    return notices;
}

void AlarmSession::reload_if_changed() {
    if (!file_.changed_on_disk()) return;
    // The file may be caught half-written, or hand-edited into a broken state: then keep the
    // alarms this session has rather than dropping them all, and leave the file alone.
    auto loaded = file_.load(StateFile::OnCorrupt::Leave);
    if (loaded.unreadable) {
        if (loaded.warning) queued_notices_.push_back({Notice::Level::Warning, *loaded.warning});
        return;
    }
    settings_ = loaded.state.settings;
    apply_settings();
    // Runtime state (ringing, snoozed) is ours; the file's copy of it is what we last wrote.
    service_.sync(loaded.state.alarms, clock_.now());
    last_saved_ = loaded.state;
    last_saved_.alarms = service_.alarms();
    last_saved_.pending = service_.pending();
    stop_audio_if_quiet();  // the ringing alarm may have been edited or removed
    queued_notices_.push_back({Notice::Level::Info, "Alarm list reloaded (changed outside this window)."});
}

std::vector<Notice> AlarmSession::tick() {
    reload_if_changed();
    std::vector<Notice> notices = std::move(queued_notices_);
    queued_notices_.clear();
    const TimePoint now = clock_.now();

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
    if (power_ != nullptr) {
        // The app won't be running to ring, so don't wake the computer for nothing.
        power_->keep_awake(false);
        power_->schedule_wake(std::nullopt);
    }
    persist();
}

void AlarmSession::snooze() {
    reload_if_changed();
    const TimePoint now = clock_.now();
    for (int id : service_.ringing()) service_.snooze(id, now);
    stop_audio_if_quiet();
}

AlarmSession::DismissResult AlarmSession::dismiss(std::optional<int> answer) {
    reload_if_changed();
    if (service_.ringing().empty()) return DismissResult::NotRinging;
    if (problem_) {
        if (!answer) return DismissResult::NeedsAnswer;
        if (*answer != problem_->answer) {
            new_problem();
            return DismissResult::WrongAnswer;
        }
    }
    const TimePoint now = clock_.now();
    for (int id : service_.ringing()) service_.dismiss(id, now);
    stop_audio_if_quiet();
    persist_if_changed();  // one-shot alarms become disabled
    return DismissResult::Dismissed;
}

void AlarmSession::new_problem() {
    const auto seed = static_cast<std::uint32_t>(clock_.now().time_since_epoch().count()) ^ (++problem_counter_ * 2654435761U);
    problem_ = make_math_problem(seed);
}

int AlarmSession::save_alarm(Alarm alarm) {
    reload_if_changed();
    const TimePoint now = clock_.now();
    alarm.updated_at = now;  // a new alarm (id 0) is numbered by the service
    const int id = service_.upsert(std::move(alarm), now);
    stop_audio_if_quiet();
    persist();
    return id;
}

bool AlarmSession::remove_alarm(int id) {
    reload_if_changed();
    const bool ok = service_.remove(id);
    stop_audio_if_quiet();
    persist();
    return ok;
}

bool AlarmSession::toggle_alarm(int id) {
    reload_if_changed();
    const Alarm* a = service_.find(id);
    if (a == nullptr) return false;
    Alarm copy = *a;
    copy.enabled = !copy.enabled;
    save_alarm(std::move(copy));
    return true;
}

void AlarmSession::set_hour_format(HourFormat fmt) {
    reload_if_changed();
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
                if (a && a->challenge == ChallengeKind::Math && !problem_) new_problem();
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
    SavedState state{
        .settings = settings_, .alarms = service_.alarms(), .last_seen = clock_.now(), .pending = service_.pending()};
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
    if (stale || last_saved_.settings != settings_ || last_saved_.alarms != service_.alarms() ||
        last_saved_.pending != service_.pending()) {
        persist();
    }
}

void AlarmSession::stop_audio_if_quiet() {
    if (service_.ringing().empty()) problem_.reset();
    if (service_.ringing().empty() && audio_.is_playing()) audio_.stop();
    update_power();
}

void AlarmSession::update_power() {
    if (power_ == nullptr) return;
    power_->keep_awake(!service_.ringing().empty());
    std::optional<TimePoint> wake;
    if (auto next = service_.next_due()) {
        // Wake a little early; if that moment has passed, the alarm time itself is still in the future.
        wake = next->second - kWakeLead > clock_.now() ? next->second - kWakeLead : next->second;
    }
    wake_ok_ = power_->schedule_wake(wake) || !wake;
}

}  // namespace ac
