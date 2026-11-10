#pragma once

#include "core/time_types.hpp"

namespace ac {

// Converts between absolute instants and local wall-clock time.
// Abstracted so scheduling logic can be tested against zones with DST rules.
class TimeZone {
public:
    virtual ~TimeZone() = default;

    [[nodiscard]] virtual LocalDateTime to_local(TimePoint t) const = 0;

    // Resolves a local time to an instant.
    //  - Ambiguous times (DST fall-back) resolve to the earlier instant.
    //  - Non-existent times (DST spring-forward gap) resolve to the instant the
    //    wall clock would show had it not jumped, i.e. shifted forward by the gap.
    [[nodiscard]] virtual TimePoint to_utc(const LocalDateTime& local) const = 0;
};

// The operating system's configured local time zone.
class SystemTimeZone final : public TimeZone {
public:
    [[nodiscard]] LocalDateTime to_local(TimePoint t) const override;
    [[nodiscard]] TimePoint to_utc(const LocalDateTime& local) const override;
};

}  // namespace ac
