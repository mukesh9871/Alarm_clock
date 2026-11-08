#pragma once

#include "core/challenge.hpp"
#include "core/days.hpp"
#include "core/time_types.hpp"

#include <string>

namespace ac {

struct Alarm {
    int id = 0;  // 0 = not yet assigned
    TimeOfDay time{};
    std::string label{};
    bool enabled = true;
    DaySet days{};            // empty = one-shot
    int snooze_minutes = 9;
    std::string sound{};      // empty = bundled default sound
    int fade_in_seconds = 30; // volume ramps from silent to full over this long
    ChallengeKind challenge = ChallengeKind::None;  // must be solved to stop (not to snooze)
    // When the alarm was created or last edited. An alarm can't have been "missed"
    // at a time before it existed, so arming never starts earlier than this.
    TimePoint updated_at{};

    [[nodiscard]] bool repeats() const { return !days.empty(); }

    friend bool operator==(const Alarm&, const Alarm&) = default;
};

}  // namespace ac
