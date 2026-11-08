#pragma once

#include "core/time_types.hpp"

namespace ac {

// Source of "now". Injected so tests can control time.
class Clock {
public:
    virtual ~Clock() = default;
    [[nodiscard]] virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
public:
    [[nodiscard]] TimePoint now() const override {
        return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    }
};

}  // namespace ac
