#pragma once

#include "core/alarm.hpp"
#include "core/time_format.hpp"
#include "core/time_types.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ac {

struct Settings {
    HourFormat hour_format = HourFormat::H24;
    int missed_grace_minutes = 30;
    int ring_timeout_minutes = 10;

    friend bool operator==(const Settings&, const Settings&) = default;
};

// Everything persisted between runs.
struct SavedState {
    Settings settings{};
    std::vector<Alarm> alarms{};
    // Last moment the app was known to be running; used to detect alarms missed while closed.
    std::optional<TimePoint> last_seen{};
    std::vector<PendingRing> pending{};

    friend bool operator==(const SavedState&, const SavedState&) = default;
};

class StateFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::string to_json(const SavedState& state);

// Throws StateFormatError with a human-readable reason on malformed input.
[[nodiscard]] SavedState state_from_json(std::string_view json);

}  // namespace ac
