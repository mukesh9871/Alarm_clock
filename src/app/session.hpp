#pragma once

#include "core/alarm_service.hpp"
#include "core/challenge.hpp"
#include "core/clock.hpp"
#include "core/saved_state.hpp"
#include "core/time_zone.hpp"
#include "platform/audio_player.hpp"
#include "platform/power.hpp"
#include "platform/state_file.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ac {

struct Notice {
    enum class Level { Info, Warning, Alarm };
    Level level;
    std::string text;
};

using SoundResolver = std::function<std::filesystem::path(const std::string&)>;

// The running alarm clock: owns the alarm service, keeps it in sync with the
// alarm file, drives audio, and turns service events into user-facing notices.
// Front ends (console, TUI) call tick() about once a second and render.
class AlarmSession {
public:
    // `power` is optional; when given, the computer is woken shortly before each alarm
    // and kept awake while one rings.
    AlarmSession(StateFile& file, const Clock& clock, const TimeZone& tz, AudioPlayer& audio, SoundResolver resolve_sound,
                 PowerManager* power = nullptr);

    // How long before an alarm the computer is woken, so audio is ready in time.
    static constexpr std::chrono::seconds kWakeLead{60};

    // Loads alarms. Alarms that came due while the app was closed are reported as missed
    // (or rung, if within the grace period). Snoozes saved by a previous run resume.
    std::vector<Notice> start();
    std::vector<Notice> tick();
    void shutdown();

    // Act on every ringing alarm.
    void snooze();

    enum class DismissResult { Dismissed, NeedsAnswer, WrongAnswer, NotRinging };
    // Stops ringing alarms. If one requires a challenge, `answer` must solve challenge();
    // a wrong answer replaces the problem with a new one.
    DismissResult dismiss(std::optional<int> answer = std::nullopt);
    // The problem to solve before the ringing alarm(s) can be stopped, if any.
    [[nodiscard]] const MathProblem* challenge() const { return problem_ ? &*problem_ : nullptr; }

    // Edits from the UI. Each first picks up changes made from another process, so
    // it can't overwrite them, and is saved immediately.
    int save_alarm(Alarm alarm);
    bool remove_alarm(int id);
    bool toggle_alarm(int id);
    void set_hour_format(HourFormat fmt);

    [[nodiscard]] const AlarmService& service() const { return service_; }
    [[nodiscard]] const Settings& settings() const { return settings_; }
    [[nodiscard]] TimePoint now() const { return clock_.now(); }
    [[nodiscard]] const TimeZone& time_zone() const { return tz_; }
    [[nodiscard]] std::string describe(int alarm_id) const;  // "07:30 Gym"
    // False if a wake timer was requested but the OS refused it.
    [[nodiscard]] bool wake_timer_ok() const { return wake_ok_; }
    [[nodiscard]] bool wake_supported() const { return power_ && power_->wake_supported(); }

private:
    std::vector<Notice> to_notices(const std::vector<AlarmEvent>& events, bool at_startup);
    void apply_settings();
    void reload_if_changed();
    void persist();
    void persist_if_changed();
    void stop_audio_if_quiet();
    void update_power();
    void new_problem();

    StateFile& file_;
    const Clock& clock_;
    const TimeZone& tz_;
    AudioPlayer& audio_;
    SoundResolver resolve_sound_;
    PowerManager* power_;
    bool wake_ok_ = true;

    AlarmService service_;
    Settings settings_;
    SavedState last_saved_;
    std::optional<std::string> save_error_;
    std::optional<MathProblem> problem_;
    std::uint32_t problem_counter_ = 0;
    std::string reported_save_error_;
    std::vector<Notice> queued_notices_;  // from reloads outside tick(), shown on the next tick
};

}  // namespace ac
