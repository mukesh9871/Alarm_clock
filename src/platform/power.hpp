#pragma once

#include "core/time_types.hpp"

#include <memory>
#include <optional>

namespace ac {

// OS power integration so a sleeping computer still wakes you up.
class PowerManager {
public:
    virtual ~PowerManager() = default;

    // Arms (or with nullopt, cancels) a timer that wakes the computer from sleep at `when`.
    // Returns false if the OS refused (unsupported hardware/OS or wake timers disabled).
    virtual bool schedule_wake(std::optional<TimePoint> when) = 0;

    // While true, prevents the computer and display from going (back) to sleep.
    virtual void keep_awake(bool on) = 0;

    // Whether schedule_wake can work at all on this platform.
    [[nodiscard]] virtual bool wake_supported() const = 0;
};

// Windows: waitable timer with resume + SetThreadExecutionState.
// Other platforms: a no-op implementation (waking requires root there).
std::unique_ptr<PowerManager> make_power_manager();

}  // namespace ac
