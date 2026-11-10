#pragma once

#include "core/time_types.hpp"

#include <chrono>
#include <vector>

namespace ac {

// Sleep runs in roughly 90-minute cycles; waking at the end of a cycle, rather than
// in the middle of deep sleep, tends to feel much better.
inline constexpr std::chrono::minutes kSleepCycle{90};
inline constexpr std::chrono::minutes kDefaultFallAsleep{15};

struct SleepOption {
    TimeOfDay time;
    int cycles;
    std::chrono::minutes sleep;  // time actually asleep
};

// Bedtimes for waking at `wake`, most sleep first (6 cycles down to 3).
[[nodiscard]] std::vector<SleepOption> bedtimes_for(TimeOfDay wake,
                                                    std::chrono::minutes fall_asleep = kDefaultFallAsleep);

// Wake-up times if you go to bed at `bed`, least sleep first (3 cycles up to 6).
[[nodiscard]] std::vector<SleepOption> wake_times_for(TimeOfDay bed,
                                                      std::chrono::minutes fall_asleep = kDefaultFallAsleep);

}  // namespace ac
