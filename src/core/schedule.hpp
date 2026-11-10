#pragma once

#include "core/days.hpp"
#include "core/time_types.hpp"
#include "core/time_zone.hpp"

namespace ac {

// The first instant strictly after `after` at which the local wall clock in `tz`
// reads `time`, on one of `days` (empty = any day). Correct across midnight and
// DST transitions:
//  - a time skipped by spring-forward fires at the shifted instant (e.g. 02:30 -> 03:30)
//  - a time repeated by fall-back fires once, at its first occurrence
[[nodiscard]] TimePoint next_occurrence(TimeOfDay time, TimePoint after, const TimeZone& tz, DaySet days = {});

}  // namespace ac
