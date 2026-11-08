#pragma once

#include "app/cli.hpp"
#include "core/alarm.hpp"
#include "core/clock.hpp"
#include "core/time_zone.hpp"
#include "platform/state_file.hpp"

#include <functional>
#include <iosfwd>
#include <optional>
#include <vector>

namespace ac {

// Applies the set fields of `o` onto `a`.
void apply_options(const cli::AlarmOptions& o, Alarm& a);

// One-shot commands that edit the alarm file and exit (everything except Run).
// Returns the process exit code.
int run_command(const cli::Command& cmd, StateFile& file, const Clock& clock, const TimeZone& tz, std::ostream& out,
                std::ostream& err);

// When an alarm will next ring, and whether that's a snooze. Default: its next scheduled occurrence.
struct NextRing {
    std::optional<TimePoint> at;
    bool snoozed = false;
};
using NextRingFn = std::function<NextRing(const Alarm&)>;

// Prints the alarm table used by `list` and the console clock.
void print_alarms(const std::vector<Alarm>& alarms, HourFormat fmt, TimePoint now, const TimeZone& tz,
                  std::ostream& out, const NextRingFn& next_ring = {});

}  // namespace ac
