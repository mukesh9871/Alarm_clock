#include "app/commands.hpp"

#include "core/schedule.hpp"
#include "core/sleep_cycles.hpp"
#include "core/time_format.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <type_traits>
#include <variant>

namespace ac {

void apply_options(const cli::AlarmOptions& o, Alarm& a) {
    if (o.time) a.time = *o.time;
    if (o.label) a.label = *o.label;
    if (o.days) a.days = *o.days;
    if (o.snooze_minutes) a.snooze_minutes = *o.snooze_minutes;
    if (o.sound) a.sound = *o.sound;
    if (o.fade_in_seconds) a.fade_in_seconds = *o.fade_in_seconds;
    if (o.challenge) a.challenge = *o.challenge;
}

void print_alarms(const std::vector<Alarm>& alarms, HourFormat fmt, TimePoint now, const TimeZone& tz,
                  std::ostream& out, const NextRingFn& next_ring) {
    if (alarms.empty()) {
        out << "No alarms. Add one with:  alarm_clock add 07:30\n";
        return;
    }
    std::vector<Alarm> sorted = alarms;
    std::sort(sorted.begin(), sorted.end(), [](const Alarm& a, const Alarm& b) { return a.time < b.time; });

    out << std::left << std::setw(4) << "ID" << std::setw(10) << "TIME" << std::setw(22) << "REPEAT" << std::setw(5)
        << "ON" << "LABEL / NEXT\n";
    for (const Alarm& a : sorted) {
        out << std::left << std::setw(4) << a.id << std::setw(10) << format_time_of_day(a.time, fmt) << std::setw(22)
            << format_days(a.days) << std::setw(5) << (a.enabled ? "yes" : "no") << a.label
            << (a.challenge == ChallengeKind::Math ? (a.label.empty() ? "[math]" : " [math]") : "");
        const NextRing next = next_ring ? next_ring(a)
                                        : NextRing{a.enabled ? std::optional{next_occurrence(a.time, now, tz, a.days)}
                                                             : std::nullopt};
        if (next.at) {
            out << (a.label.empty() ? "" : "  ") << "(" << (next.snoozed ? "snoozed until " : "")
                << (next.snoozed ? format_time_of_day(tz.to_local(*next.at).time_of_day(), fmt)
                                 : format_date(tz.to_local(*next.at)))
                << ", in " << format_duration(*next.at - now) << ")";
        }
        out << '\n';
    }
}

void print_bedtimes(const cli::Bedtime& b, HourFormat fmt, TimeOfDay now, std::ostream& out) {
    auto line = [&](const SleepOption& o) {
        out << "  " << std::left << std::setw(10) << format_time_of_day(o.time, fmt) << o.cycles << " cycles, "
            << format_duration(o.sleep) << " of sleep" << (o.cycles >= 5 ? "  (recommended)" : "") << '\n';
    };
    if (b.wake) {
        out << "To wake up at " << format_time_of_day(*b.wake, fmt) << " between sleep cycles, go to bed at:\n";
        for (const auto& o : bedtimes_for(*b.wake)) line(o);
    } else {
        out << "If you go to bed now (" << format_time_of_day(now, fmt) << "), wake up at:\n";
        for (const auto& o : wake_times_for(now)) line(o);
    }
    out << "(Assumes ~" << kDefaultFallAsleep.count() << " minutes to fall asleep and 90-minute cycles.)\n";
}

int run_command(const cli::Command& cmd, StateFile& file, const Clock& clock, const TimeZone& tz, std::ostream& out,
                std::ostream& err) {
    auto loaded = file.load();
    if (loaded.warning) err << "Warning: " << *loaded.warning << '\n';
    SavedState& state = loaded.state;
    const TimePoint now = clock.now();

    auto find = [&](int id) -> Alarm* {
        const auto it = std::find_if(state.alarms.begin(), state.alarms.end(), [id](const Alarm& a) { return a.id == id; });
        return it == state.alarms.end() ? nullptr : &*it;
    };
    auto not_found = [&](int id) {
        err << "No alarm with id " << id << ". Run 'alarm_clock list' to see ids.\n";
        return 1;
    };
    auto save = [&] { file.save(state); };
    // A changed alarm starts fresh: forget any snooze the running clock saved for it.
    auto forget_pending = [&](int id) {
        std::erase_if(state.pending, [id](const PendingRing& p) { return p.alarm_id == id; });
    };

    return std::visit(
        [&](const auto& c) -> int {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, cli::List>) {
                print_alarms(state.alarms, state.settings.hour_format, now, tz, out, [&](const Alarm& a) {
                    for (const PendingRing& p : state.pending) {
                        if (p.alarm_id == a.id && a.enabled) return NextRing{p.due, true};
                    }
                    return NextRing{a.enabled ? std::optional{next_occurrence(a.time, now, tz, a.days)} : std::nullopt};
                });
                return 0;
            } else if constexpr (std::is_same_v<T, cli::Add>) {
                Alarm a;
                a.id = next_alarm_id(state.alarms);
                apply_options(c.options, a);
                a.updated_at = now;
                state.alarms.push_back(a);
                save();
                const TimePoint next = next_occurrence(a.time, now, tz, a.days);
                out << "Added alarm " << a.id << ": " << format_time_of_day(a.time, state.settings.hour_format) << ", "
                    << format_days(a.days) << " - rings in " << format_duration(next - now) << ".\n";
                return 0;
            } else if constexpr (std::is_same_v<T, cli::Edit>) {
                Alarm* a = find(c.id);
                if (a == nullptr) return not_found(c.id);
                const Alarm before = *a;
                apply_options(c.options, *a);
                if (schedule_changed(before, *a)) a->enabled = true;
                a->updated_at = now;
                forget_pending(c.id);
                save();
                out << "Updated alarm " << c.id;
                if (a->enabled) {
                    out << " - rings in " << format_duration(next_occurrence(a->time, now, tz, a->days) - now) << ".\n";
                } else {
                    out << ". It is off; turn it on with: alarm_clock enable " << c.id << '\n';
                }
                return 0;
            } else if constexpr (std::is_same_v<T, cli::Remove>) {
                if (find(c.id) == nullptr) return not_found(c.id);
                std::erase_if(state.alarms, [&](const Alarm& a) { return a.id == c.id; });
                forget_pending(c.id);
                save();
                out << "Removed alarm " << c.id << ".\n";
                return 0;
            } else if constexpr (std::is_same_v<T, cli::SetEnabled>) {
                Alarm* a = find(c.id);
                if (a == nullptr) return not_found(c.id);
                a->enabled = c.enabled;
                a->updated_at = now;
                forget_pending(c.id);
                save();
                out << "Alarm " << c.id << (c.enabled ? " enabled.\n" : " disabled.\n");
                return 0;
            } else if constexpr (std::is_same_v<T, cli::Bedtime>) {
                print_bedtimes(c, state.settings.hour_format, tz.to_local(now).time_of_day(), out);
                return 0;
            } else if constexpr (std::is_same_v<T, cli::SetHourFormat>) {
                state.settings.hour_format = c.format;
                save();
                out << "Using " << (c.format == HourFormat::H12 ? "12" : "24") << "-hour time.\n";
                return 0;
            } else {
                err << "internal error: command not handled here\n";
                return 2;
            }
        },
        cmd);
}

}  // namespace ac
