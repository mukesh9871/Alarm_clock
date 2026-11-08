#pragma once

#include "core/time_types.hpp"

#include <string>

namespace ac {

struct Alarm {
    int id = 0;  // 0 = not yet assigned
    TimeOfDay time{};
    std::string label{};
    bool enabled = true;

    friend bool operator==(const Alarm&, const Alarm&) = default;
};

}  // namespace ac
