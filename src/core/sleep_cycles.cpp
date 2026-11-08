#include "core/sleep_cycles.hpp"

namespace ac {

namespace {

constexpr int kMinutesPerDay = 24 * 60;

TimeOfDay add_minutes(TimeOfDay t, long long minutes) {
    long long total = (t.hour * 60 + t.minute + minutes) % kMinutesPerDay;
    if (total < 0) total += kMinutesPerDay;
    return {static_cast<int>(total / 60), static_cast<int>(total % 60)};
}

}  // namespace

std::vector<SleepOption> bedtimes_for(TimeOfDay wake, std::chrono::minutes fall_asleep) {
    std::vector<SleepOption> out;
    for (int cycles = 6; cycles >= 3; --cycles) {
        const auto sleep = kSleepCycle * cycles;
        out.push_back({add_minutes(wake, -(sleep + fall_asleep).count()), cycles, sleep});
    }
    return out;
}

std::vector<SleepOption> wake_times_for(TimeOfDay bed, std::chrono::minutes fall_asleep) {
    std::vector<SleepOption> out;
    for (int cycles = 3; cycles <= 6; ++cycles) {
        const auto sleep = kSleepCycle * cycles;
        out.push_back({add_minutes(bed, (sleep + fall_asleep).count()), cycles, sleep});
    }
    return out;
}

}  // namespace ac
