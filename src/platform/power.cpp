#include "platform/power.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ac {

namespace {

#ifdef _WIN32

class WindowsPower final : public PowerManager {
public:
    WindowsPower() : timer_(CreateWaitableTimerW(nullptr, TRUE, nullptr)) {}

    ~WindowsPower() override {
        keep_awake(false);
        if (timer_) {
            CancelWaitableTimer(timer_);
            CloseHandle(timer_);
        }
    }

    bool schedule_wake(std::optional<TimePoint> when) override {
        if (!timer_) return false;
        if (when == armed_) return armed_ok_;
        armed_ = when;
        if (!when) {
            CancelWaitableTimer(timer_);
            armed_ok_ = true;
            return true;
        }
        // Absolute due time in FILETIME units: 100 ns intervals since 1601-01-01 UTC.
        constexpr long long kEpochDiff = 11'644'473'600LL;
        LARGE_INTEGER due;
        due.QuadPart = (when->time_since_epoch().count() + kEpochDiff) * 10'000'000LL;
        // fResume = TRUE asks Windows to wake the machine from sleep when the timer fires.
        SetLastError(ERROR_SUCCESS);
        const BOOL ok = SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, TRUE);
        armed_ok_ = ok && GetLastError() != ERROR_NOT_SUPPORTED;
        return armed_ok_;
    }

    void keep_awake(bool on) override {
        if (on == awake_) return;
        awake_ = on;
        SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS);
    }

    bool wake_supported() const override { return timer_ != nullptr; }

private:
    HANDLE timer_;
    std::optional<TimePoint> armed_;
    bool armed_ok_ = true;
    bool awake_ = false;
};

#endif

class NoPower final : public PowerManager {
public:
    bool schedule_wake(std::optional<TimePoint>) override { return false; }
    void keep_awake(bool) override {}
    bool wake_supported() const override { return false; }
};

}  // namespace

std::unique_ptr<PowerManager> make_power_manager() {
#ifdef _WIN32
    return std::make_unique<WindowsPower>();
#else
    return std::make_unique<NoPower>();
#endif
}

}  // namespace ac
